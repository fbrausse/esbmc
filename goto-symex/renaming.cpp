#include <irep2.h>
#include <migrate.h>
#include <prefix.h>

#include <langapi/language_util.h>

#include "renaming.h"

unsigned renaming::level2t::current_number(const expr2tc &symbol) const
{
  return current_number(name_record(to_symbol2t(symbol)));
}

unsigned renaming::level2t::current_number(const name_record &symbol) const
{
  current_namest::const_iterator it = current_names.find(symbol);
  if (it == current_names.end()) return 0;
  return it->second.count;
}

unsigned int
renaming::level1t::current_number(const irep_idt &name) const
{
  current_namest::const_iterator it = current_names.find(name_record(name));
  if (it == current_names.end()) return 0;
  return it->second;
}

void
renaming::level1t::get_ident_name(expr2tc &sym) const
{
  symbol2t &symbol = to_symbol2t(sym);

  current_namest::const_iterator it =
    current_names.find(name_record(to_symbol2t(sym)));

  if (it == current_names.end())
  {
    // can not find; it's a global symbol.
    symbol.rlevel = symbol2t::level1_global;
    return;
  }

  symbol.rlevel = symbol2t::level1;
  symbol.level1_num = it->second;
  symbol.thread_num = thread_id;
  return;
}

void
renaming::level2t::get_ident_name(expr2tc &sym) const
{
  symbol2t &symbol = to_symbol2t(sym);

  current_namest::const_iterator it = current_names.find(name_record(symbol));

  symbol2t::renaming_level lev = symbol.rlevel =
              (symbol.rlevel == symbol2t::level1) ? symbol2t::level2
                                                  : symbol2t::level2_global;

  if (it == current_names.end()) {
    // Un-numbered so far.
    symbol.rlevel = lev;
    symbol.level2_num = 0;
    symbol.node_num = 0;
    return;
  }

  symbol.rlevel = lev;
  symbol.level2_num = it->second.count;
  symbol.node_num = it->second.node_id;
  return;
}

void renaming::level1t::rename(expr2tc &expr) const
{
  // rename all the symbols with their last known value

  if (is_nil_expr(expr))
    return;

  if (is_symbol2t(expr))
  {
    symbol2t &sym = to_symbol2t(expr);

    // first see if it's already an l1 name

    if (sym.rlevel != symbol2t::level0)
      return;

    const current_namest::const_iterator it =
      current_names.find(name_record(sym));

    if (it != current_names.end()) {
      expr = symbol2tc(sym.type, sym.thename, symbol2t::level1,
                       it->second, 0, thread_id, 0);
    } else {
      // This isn't an l1 declared name, so it's a global.
      to_symbol2t(expr).rlevel = symbol2t::level1_global;
    }
  }
  else if (is_address_of2t(expr))
  {
    rename(to_address_of2t(expr).ptr_obj);
  }
  else
  {
    // do this recursively
    Forall_operands2(it, idx, expr)
      rename(*it);
  }
}

void renaming::level2t::rename(expr2tc &expr) const
{
  // rename all the symbols with their last known value

  if (is_symbol2t(expr))
  {
    symbol2t &sym = to_symbol2t(expr);

    // first see if it's already an l2 name

    if (sym.rlevel == symbol2t::level2 || sym.rlevel == symbol2t::level2_global)
      return;

    if (sym.thename == "NULL")
      return;
    if (sym.thename == "INVALID")
      return;
    if (has_prefix(sym.thename.as_string(), "nondet$"))
      return;

    const current_namest::const_iterator it =
      current_names.find(name_record(sym));

    if(it!=current_names.end())
    {
      // Is this a global symbol? Gets renamed differently.
      symbol2t::renaming_level lev;
      if (sym.rlevel == symbol2t::level0 ||
          sym.rlevel == symbol2t::level1_global)
        lev = symbol2t::level2_global;
      else
        lev = symbol2t::level2;

      if (!is_nil_expr(it->second.constant))
        expr = it->second.constant; // sym is now invalid reference
      else
        expr = symbol2tc(sym.type, sym.thename, lev,
                         sym.level1_num, it->second.count,
                         sym.thread_num, it->second.node_id);
    }
    else
    {
      symbol2t::renaming_level lev;
      if (sym.rlevel == symbol2t::level0 ||
          sym.rlevel == symbol2t::level1_global)
        lev = symbol2t::level2_global;
      else
        lev = symbol2t::level2;

      expr = symbol2tc(sym.type, sym.thename, lev, sym.level1_num,
                       0, sym.thread_num, 0);
    }
  }
  else if (is_address_of2t(expr))
  {
    // do nothing
  }
  else
  {
    // do this recursively
    Forall_operands2(it, idx, expr)
      if (!is_nil_expr(*it))
        rename(*it);
  }
}

void renaming::level2t::coveredinbees(expr2tc &lhs_sym, unsigned count, unsigned node_id)
{
#ifndef NDEBUG
  symbol2t &sym = to_symbol2t(lhs_sym);
  assert(sym.rlevel == symbol2t::level1 ||
         sym.rlevel == symbol2t::level1_global);
#endif

  valuet &entry=current_names[name_record(to_symbol2t(lhs_sym))];
  assert(entry.count <= count);
  entry.count=count;
  entry.node_id = node_id;
}

void renaming::renaming_levelt::get_original_name(expr2tc &expr,
                                            symbol2t::renaming_level lev) const
{

  if (is_nil_expr(expr))
    return;

  Forall_operands2(it, idx, expr)
    get_original_name(*it);

  if (is_symbol2t(expr))
  {
    symbol2t &sym = to_symbol2t(expr);

    // Rename level2_global down to level1_global, not level1
    if (lev == symbol2t::level1 && sym.rlevel == symbol2t::level2_global)
      lev = symbol2t::level1_global;
    // level1 and level1_global are equivalent.
    else if (lev == symbol2t::level1 && sym.rlevel == symbol2t::level1_global)
      return;

    // Can't rename any lower,
    if (sym.rlevel == symbol2t::level0)
      return;

    // Wipe out some data with default values and set renaming level to whatever
    // was requested.
    switch (lev) {
    case symbol2t::level1:
    case symbol2t::level1_global:
      sym.rlevel = lev;
      sym.node_num = 0;
      sym.level2_num = 0;
      return;
    case symbol2t::level0:
      sym.rlevel = lev;
      sym.node_num = 0;
      sym.level2_num = 0;
      sym.thread_num = 0;
      sym.level1_num = 0;
      return;
    default:
      std::cerr << "get_original_nameing to invalid level " << lev << std::endl;
      abort();
    }
  }
}

void renaming::level1t::print(std::ostream &out) const
{
  for(current_namest::const_iterator
      it=current_names.begin();
      it!=current_names.end();
      it++)
    out << it->first.base_name << " --> "
        << "thread " << thread_id << " count " << it->second << std::endl;
}

void renaming::level2t::print(std::ostream &out) const
{
  for(current_namest::const_iterator
      it=current_names.begin();
      it!=current_names.end();
      it++) {
    out << it->first.base_name;

    if (it->first.lev == symbol2t::level1)
      out << "@" << it->first.l1_num <<  "!" << it->first.t_num;

    out <<  " --> ";

    if (!is_nil_expr(it->second.constant)) {
      out << from_expr(it->second.constant) << std::endl;
    } else {
      out << "node " << it->second.node_id << " num " << it->second.count;
      out  << std::endl;
    }
  }
}

void renaming::level2t::dump() const
{

  print(std::cout);
}

void
renaming::level2t::make_assignment(expr2tc &lhs_symbol,
                                   const expr2tc &const_value,
                          const expr2tc &assigned_value __attribute__((unused)))
{
  assert(to_symbol2t(lhs_symbol).rlevel == symbol2t::level1 ||
         to_symbol2t(lhs_symbol).rlevel == symbol2t::level1_global);
  valuet &entry = current_names[name_record(to_symbol2t(lhs_symbol))];

  // This'll update entry beneath our feet; could reengineer it in the future.
  rename(lhs_symbol, entry.count + 1);

  symbol2t &symbol = to_symbol2t(lhs_symbol);
  symbol2t::renaming_level lev = (symbol.rlevel == symbol2t::level0 ||
                                  symbol.rlevel == symbol2t::level1_global)
                                  ? symbol2t::level2_global : symbol2t::level2;
  symbol.rlevel = lev;
  // These fields were updated by the rename call,
  symbol.level2_num = entry.count; 
  symbol.node_num = entry.node_id;

  entry.constant = const_value;
}
