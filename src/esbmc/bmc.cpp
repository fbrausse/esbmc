/*******************************************************************\

Module: Symbolic Execution of ANSI-C

Authors: Daniel Kroening, kroening@kroening.com
         Lucas Cordeiro, lcc08r@ecs.soton.ac.uk

\*******************************************************************/

#include <csignal>
#include <sys/types.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#include <winbase.h>
#undef ERROR
#undef small
#endif

#include <fmt/format.h>
#include <ac_config.h>
#include <esbmc/bmc.h>
#include <esbmc/document_subgoals.h>
#include <fstream>
#include <goto-programs/goto_loops.h>
#include <goto-symex/build_goto_trace.h>
#include <goto-symex/goto_trace.h>
#include <goto-symex/reachability_tree.h>
#include <goto-symex/slice.h>
#include <goto-symex/xml_goto_trace.h>
#include <langapi/language_util.h>
#include <langapi/languages.h>
#include <langapi/mode.h>
#include <sstream>
#include <util/i2string.h>
#include <irep2/irep2.h>
#include <util/location.h>
#include <util/message/message_stream.h>
#include <util/message/format.h>
#include <util/migrate.h>
#include <util/show_symbol_table.h>
#include <util/time_stopping.h>

bmct::bmct(
  goto_functionst &funcs,
  optionst &opts,
  contextt &_context,
  const messaget &_message_handler)
  : options(opts), context(_context), ns(context), msg(_message_handler)
{
  interleaving_number = 0;
  interleaving_failed = 0;

  if(options.get_bool_option("smt-during-symex"))
  {
    runtime_solver =
      std::shared_ptr<smt_convt>(create_solver("", ns, options, msg));

    symex = std::make_shared<reachability_treet>(
      funcs,
      ns,
      options,
      std::shared_ptr<runtime_encoded_equationt>(
        new runtime_encoded_equationt(ns, *runtime_solver, msg)),
      _context,
      _message_handler);
  }
  else
  {
    symex = std::make_shared<reachability_treet>(
      funcs,
      ns,
      options,
      std::shared_ptr<symex_target_equationt>(
        new symex_target_equationt(ns, msg)),
      _context,
      _message_handler);
  }
}

void bmct::do_cbmc(
  std::shared_ptr<smt_convt> &smt_conv,
  std::shared_ptr<symex_target_equationt> &eq)
{
  eq->convert(*smt_conv.get());
}

void bmct::successful_trace()
{
  if(options.get_bool_option("result-only"))
    return;

  std::string witness_output = options.get_option("witness-output");
  if(witness_output != "")
  {
    goto_tracet goto_trace;
    msg.status("Building successful trace");
    /* build_successful_goto_trace(eq, ns, goto_trace); */
    correctness_graphml_goto_trace(options, ns, goto_trace, msg);
  }
}

void bmct::error_trace(
  std::shared_ptr<smt_convt> &smt_conv,
  std::shared_ptr<symex_target_equationt> &eq)
{
  if(options.get_bool_option("result-only"))
    return;

  msg.status("Building error trace");

  bool is_compact_trace = true;
  if(
    options.get_bool_option("no-slice") &&
    !options.get_bool_option("compact-trace"))
    is_compact_trace = false;

  goto_tracet goto_trace;
  build_goto_trace(eq, smt_conv, goto_trace, is_compact_trace, msg);

  std::string output_file = options.get_option("cex-output");
  if(output_file != "")
  {
    std::ofstream out(output_file);
    show_goto_trace(out, ns, goto_trace, msg);
  }

  std::string witness_output = options.get_option("witness-output");
  if(witness_output != "")
    violation_graphml_goto_trace(options, ns, goto_trace, msg);

  std::ostringstream oss;
  oss << "\nCounterexample:\n";
  show_goto_trace(oss, ns, goto_trace, msg);
  msg.result(oss.str());
}

smt_convt::resultt bmct::run_decision_procedure(
  std::shared_ptr<smt_convt> &smt_conv,
  std::shared_ptr<symex_target_equationt> &eq)
{
  std::string logic;

  if(!options.get_bool_option("int-encoding"))
  {
    logic = "bit-vector";
    logic += (!config.ansi_c.use_fixed_for_float) ? "/floating-point " : " ";
    logic += "arithmetic";
  }
  else
    logic = "integer/real arithmetic";

  msg.status(fmt::format("Encoding remaining VCC(s) using {}", logic));

  fine_timet encode_start = current_time();
  do_cbmc(smt_conv, eq);
  fine_timet encode_stop = current_time();

  std::ostringstream str;
  str << "Encoding to solver time: ";
  output_time(encode_stop - encode_start, str);
  str << "s";
  msg.status(str.str());

  if(
    options.get_bool_option("smt-formula-too") ||
    options.get_bool_option("smt-formula-only"))
  {
    smt_conv->dump_smt();
    if(options.get_bool_option("smt-formula-only"))
      return smt_convt::P_SMTLIB;
  }

  std::stringstream ss;
  ss << "Solving with solver " << smt_conv->solver_text();
  msg.status(ss.str());

  fine_timet sat_start = current_time();
  smt_convt::resultt dec_result = smt_conv->dec_solve();
  fine_timet sat_stop = current_time();

  // output runtime
  str.clear();
  str << "\nRuntime decision procedure: ";
  output_time(sat_stop - sat_start, str);
  str << "s";
  msg.status(str.str());

  return dec_result;
}

void bmct::report_success()
{
  msg.status("\nVERIFICATION SUCCESSFUL");
}

void bmct::report_failure()
{
  msg.status("\nVERIFICATION FAILED");
}

void bmct::show_program(std::shared_ptr<symex_target_equationt> &eq)
{
  unsigned int count = 1;
  std::ostringstream oss;
  if(config.options.get_bool_option("ssa-symbol-table"))
    ::show_symbol_table_plain(ns, oss, msg);

  languagest languages(ns, language_idt::C, msg);

  oss << "\nProgram constraints: \n";

  bool sliced = config.options.get_bool_option("ssa-sliced");

  for(auto const &it : eq->SSA_steps)
  {
    if(!(it.is_assert() || it.is_assignment() || it.is_assume()))
      continue;

    if(it.ignore && !sliced)
      continue;

    oss << "// " << it.source.pc->location_number << " ";
    oss << it.source.pc->location.as_string();
    if(!it.comment.empty())
      oss << " (" << it.comment << ")";
    oss << "\n/* " << count << " */ ";

    std::string string_value;
    languages.from_expr(migrate_expr_back(it.cond), string_value);

    if(it.is_assignment())
    {
      oss << string_value << "\n";
    }
    else if(it.is_assert())
    {
      oss << "(assert)" << string_value << "\n";
    }
    else if(it.is_assume())
    {
      oss << "(assume)" << string_value << "\n";
    }
    else if(it.is_renumber())
    {
      oss << "renumber: " << from_expr(ns, "", it.lhs, msg) << "\n";
    }

    if(!migrate_expr_back(it.guard).is_true())
    {
      languages.from_expr(migrate_expr_back(it.guard), string_value);
      oss << std::string(i2string(count).size() + 3, ' ');
      oss << "guard: " << string_value << "\n";
    }

    oss << '\n';
    count++;
  }
  msg.status(oss.str());
}

void bmct::report_trace(
  smt_convt::resultt &res,
  std::shared_ptr<symex_target_equationt> &eq)
{
  bool bs = options.get_bool_option("base-case");
  bool fc = options.get_bool_option("forward-condition");
  bool is = options.get_bool_option("inductive-step");
  bool term = options.get_bool_option("termination");
  bool show_cex = options.get_bool_option("show-cex");

  switch(res)
  {
  case smt_convt::P_UNSATISFIABLE:
    if(is && term)
    {
    }
    else if(!bs)
    {
      successful_trace();
    }
    break;

  case smt_convt::P_SATISFIABLE:
    if(!bs && show_cex)
    {
      error_trace(runtime_solver, eq);
    }
    else if(!is && !fc)
    {
      error_trace(runtime_solver, eq);
    }
    break;

  default:
    break;
  }
}

void bmct::report_result(smt_convt::resultt &res)
{
  bool bs = options.get_bool_option("base-case");
  bool fc = options.get_bool_option("forward-condition");
  bool is = options.get_bool_option("inductive-step");
  bool term = options.get_bool_option("termination");

  switch(res)
  {
  case smt_convt::P_UNSATISFIABLE:
    if(is && term)
    {
      report_failure();
    }
    else if(!bs)
    {
      report_success();
    }
    else
    {
      msg.status("No bug has been found in the base case");
    }
    break;

  case smt_convt::P_SATISFIABLE:
    if(!is && !fc)
    {
      report_failure();
    }
    else if(fc)
    {
      msg.status("The forward condition is unable to prove the property");
    }
    else if(is)
    {
      msg.status("The inductive step is unable to prove the property");
    }
    break;

  // Return failure if we didn't actually check anything, we just emitted the
  // test information to an SMTLIB formatted file. Causes esbmc to quit
  // immediately (with no error reported)
  case smt_convt::P_SMTLIB:
    return;

  default:
    msg.error("SMT solver failed");
    break;
  }

  if((interleaving_number > 0) && options.get_bool_option("all-runs"))
  {
    msg.status(
      "Number of generated interleavings: " +
      integer2string((interleaving_number)));
    msg.status(
      "Number of failed interleavings: " +
      integer2string((interleaving_failed)));
  }
}

smt_convt::resultt bmct::start_bmc()
{
  std::shared_ptr<symex_target_equationt> eq;
  smt_convt::resultt res = run(eq);
  report_trace(res, eq);
  report_result(res);
  return res;
}

smt_convt::resultt bmct::run(std::shared_ptr<symex_target_equationt> &eq)
{
  symex->options.set_option("unwind", options.get_option("unwind"));
  symex->setup_for_new_explore();

  if(options.get_bool_option("schedule"))
    return run_thread(eq);

  smt_convt::resultt res;
  do
  {
    if(++interleaving_number > 1)
      msg.status(
        fmt::format("*** Thread interleavings {} ***", interleaving_number));

    fine_timet bmc_start = current_time();
    res = run_thread(eq);

    if(res == smt_convt::P_SATISFIABLE)
    {
      if(config.options.get_bool_option("smt-model"))
        runtime_solver->print_model();

      if(config.options.get_bool_option("bidirectional"))
        bidirectional_search(runtime_solver, eq);
    }

    if(res)
    {
      if(res == smt_convt::P_SATISFIABLE)
        ++interleaving_failed;

      if(!options.get_bool_option("all-runs"))
        return res;
    }
    fine_timet bmc_stop = current_time();

    std::ostringstream str;
    str << "BMC program time: ";
    output_time(bmc_stop - bmc_start, str);
    str << "s";
    msg.status(str.str());

    // Only run for one run
    if(options.get_bool_option("interactive-ileaves"))
      return res;

  } while(symex->setup_next_formula());

  return interleaving_failed > 0 ? smt_convt::P_SATISFIABLE : res;
}

void bmct::bidirectional_search(
  std::shared_ptr<smt_convt> &smt_conv,
  std::shared_ptr<symex_target_equationt> &eq)
{
  // We should only analyse the inductive step's cex and we're running
  // in k-induction mode
  if(!(options.get_bool_option("inductive-step") &&
       options.get_bool_option("k-induction")))
    return;

  // We'll walk list of SSA steps and look for inductive assignments
  std::vector<stack_framet> frames;
  unsigned assert_loop_number = 0;
  for(auto ssait : eq->SSA_steps)
  {
    if(ssait.is_assert() && smt_conv->l_get(ssait.cond_ast).is_false())
    {
      if(!ssait.loop_number)
        return;

      // Save the location of the failed assertion
      frames = ssait.stack_trace;
      assert_loop_number = ssait.loop_number;

      // We are not interested in instructions before the failed assertion yet
      break;
    }
  }

  for(auto f : frames)
  {
    // Look for the function
    goto_functionst::function_mapt::iterator fit =
      symex->goto_functions.function_map.find(f.function);
    assert(fit != symex->goto_functions.function_map.end());

    // Find function loops
    goto_loopst loops(f.function, symex->goto_functions, fit->second, msg);

    if(!loops.get_loops().size())
      continue;

    auto lit = loops.get_loops().begin(), lie = loops.get_loops().end();
    while(lit != lie)
    {
      auto loop_head = lit->get_original_loop_head();

      // Skip constraints from other loops
      if(loop_head->loop_number == assert_loop_number)
        break;

      ++lit;
    }

    if(lit == lie)
      continue;

    // Get the loop vars
    auto all_loop_vars = lit->get_modified_loop_vars();
    all_loop_vars.insert(
      lit->get_unmodified_loop_vars().begin(),
      lit->get_unmodified_loop_vars().end());

    // Now, walk the SSA and get the last value of each variable before the loop
    std::unordered_map<irep_idt, std::pair<expr2tc, expr2tc>, irep_id_hash>
      var_ssa_list;

    for(auto ssait : eq->SSA_steps)
    {
      if(ssait.loop_number == lit->get_original_loop_head()->loop_number)
        break;

      if(ssait.ignore)
        continue;

      if(!ssait.is_assignment())
        continue;

      expr2tc new_lhs = ssait.original_lhs;
      renaming::renaming_levelt::get_original_name(
        new_lhs, symbol2t::level0, msg);

      if(all_loop_vars.find(new_lhs) == all_loop_vars.end())
        continue;

      var_ssa_list[to_symbol2t(new_lhs).thename] = {
        ssait.original_lhs, ssait.rhs};
    }

    if(!var_ssa_list.size())
      return;

    // Query the solver for the value of each variable
    std::vector<expr2tc> equalities;
    for(auto it : var_ssa_list)
    {
      // We don't support arrays or pointers
      if(is_array_type(it.second.first) || is_pointer_type(it.second.first))
        return;

      auto lhs = build_lhs(smt_conv, it.second.first, msg);
      auto value = build_rhs(smt_conv, it.second.second, msg);

      // Add lhs and rhs to the list of new constraints
      equalities.push_back(equality2tc(lhs, value));
    }

    // Build new assertion
    expr2tc constraints = equalities[0];
    for(std::size_t i = 1; i < equalities.size(); ++i)
      constraints = and2tc(constraints, equalities[i]);

    // and add it to the goto program
    goto_programt::targett loop_exit = lit->get_original_loop_exit();

    goto_programt::instructiont i;
    i.make_assertion(not2tc(constraints));
    i.location = loop_exit->location;
    i.location.user_provided(true);
    i.loop_number = loop_exit->loop_number;
    i.inductive_assertion = true;

    fit->second.body.insert_swap(loop_exit, i);

    // recalculate numbers, etc.
    symex->goto_functions.update();
    return;
  }
}

smt_convt::resultt bmct::run_thread(std::shared_ptr<symex_target_equationt> &eq)
{
  std::shared_ptr<goto_symext::symex_resultt> result;

  fine_timet symex_start = current_time();
  try
  {
    if(options.get_bool_option("schedule"))
    {
      result = symex->generate_schedule_formula();
    }
    else
    {
      result = symex->get_next_formula();
    }
  }

  catch(std::string &error_str)
  {
    msg.error(error_str);
    return smt_convt::P_ERROR;
  }

  catch(const char *error_str)
  {
    msg.error(error_str);
    return smt_convt::P_ERROR;
  }

  catch(std::bad_alloc &)
  {
    msg.error("Out of memory\n");
    return smt_convt::P_ERROR;
  }

  fine_timet symex_stop = current_time();

  eq = std::dynamic_pointer_cast<symex_target_equationt>(result->target);

  {
    std::ostringstream str;
    str << "Symex completed in: ";
    output_time(symex_stop - symex_start, str);
    str << "s";
    str << " (" << eq->SSA_steps.size() << " assignments)";
    msg.status(str.str());
  }

  if(options.get_bool_option("double-assign-check"))
    eq->check_for_duplicate_assigns();

  try
  {
    fine_timet slice_start = current_time();
    BigInt ignored;
    if(!options.get_bool_option("no-slice"))
      ignored = slice(eq, options.get_bool_option("slice-assumes"));
    else
      ignored = simple_slice(eq);
    fine_timet slice_stop = current_time();

    {
      std::ostringstream str;
      str << "Slicing time: ";
      output_time(slice_stop - slice_start, str);
      str << "s";
      str << " (removed " << ignored << " assignments)";
      msg.status(str.str());
    }

    if(
      options.get_bool_option("program-only") ||
      options.get_bool_option("program-too"))
      show_program(eq);

    if(options.get_bool_option("program-only"))
      return smt_convt::P_SMTLIB;

    {
      std::ostringstream str;
      str << "Generated " << result->total_claims << " VCC(s), ";
      str << result->remaining_claims << " remaining after simplification ";
      str << "(" << BigInt(eq->SSA_steps.size()) - ignored << " assignments)";
      msg.status(str.str());
    }

    if(options.get_bool_option("document-subgoals"))
    {
      std::ostringstream oss;
      document_subgoals(*eq.get(), oss);
      msg.status(oss.str());
      return smt_convt::P_SMTLIB;
    }

    if(options.get_bool_option("show-vcc"))
    {
      show_vcc(eq);
      return smt_convt::P_SMTLIB;
    }

    if(result->remaining_claims == 0)
    {
      if(options.get_bool_option("smt-formula-only"))
      {
        msg.status(
          "No VCC remaining, no SMT formula will be generated for"
          " this program\n");
        return smt_convt::P_SMTLIB;
      }

      return smt_convt::P_UNSATISFIABLE;
    }

    if(!options.get_bool_option("smt-during-symex"))
    {
      runtime_solver =
        std::shared_ptr<smt_convt>(create_solver("", ns, options, msg));
    }

    return run_decision_procedure(runtime_solver, eq);
  }

  catch(std::string &error_str)
  {
    msg.error(error_str);
    return smt_convt::P_ERROR;
  }

  catch(const char *error_str)
  {
    msg.error(error_str);
    return smt_convt::P_ERROR;
  }

  catch(std::bad_alloc &)
  {
    msg.error("Out of memory\n");
    return smt_convt::P_ERROR;
  }
}
