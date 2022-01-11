#include <esbmc/esbmc_parseoptions.h>
#include <fstream>
#include <util/cmdline.h>

const struct group_opt_templ all_cmd_options[] = {
  {"Main Usage",
   {{"input-file",
     boost::program_options::value<std::vector<std::string>>()->value_name(
       "file.c ..."),
     "source file names"}}},
  {"Options", {{"help,?", NULL, "show help"}}},
  {"Printing options",
   {{"symbol-table-only", NULL, "only show symbol table"},
    {"symbol-table-too", NULL, "show symbol table and verify"},
    {"parse-tree-only", NULL, "only show parse tree"},
    {"parse-tree-too", NULL, "show parse tree and verify"},
    {"goto-functions-only", NULL, "only show goto program"},
    {"goto-functions-too", NULL, "show goto program and verify"},
    {"program-only", NULL, "only show program expression"},
    {"program-too", NULL, "show program expression and verify"},
    {"ssa-symbol-table", NULL, "show symbol table along with SSA"},
    {"ssa-guards", NULL, ""},
    {"ssa-sliced", NULL, "print the sliced SSAs"},
    {"ssa-no-location", NULL, ""},
    {"smt-formula-only",
     NULL,
     "only show SMT formula {not supported by all solvers},"},
    {"smt-formula-too ",
     NULL,
     "show SMT formula (not supported by all solvers), and verify"},
    {"smt-model",
     boost::program_options::value<std::string>()->value_name("path"),
     "print SMT model (not supported by all solvers), if the formula is SAT"}}},
  {"Trace",
   {{"quiet",
     NULL,
     "do not print unwinding information during symbolic execution"},
    {"compact-trace", NULL, ""},
    {"symex-trace", NULL, "print instructions during symbolic execution"},
    {"ssa-trace", NULL, "print SSA during SMT encoding"},
    {"ssa-smt-trace", NULL, "print generated SMT during SMT encoding"},
    {"symex-ssa-trace", NULL, "print generated SSA during symbolic execution"},
    {"show-goto-value-sets",
     NULL,
     "show value-set analysis for the goto functions"},
    {"show-symex-value-sets",
     NULL,
     "show value-set analysis during symbolic execution"}}},
  {"Frontend",
   {{"include,I",
     boost::program_options::value<std::vector<std::string>>()->value_name(
       "path"),
     "set include path"},
    {"define,D",
     boost::program_options::value<std::vector<std::string>>()->value_name(
       "macro"),
     "define preprocessor macro"},
    {"warning,W",
     boost::program_options::value<std::vector<std::string>>(),
     ""},
    {"force,f", boost::program_options::value<std::vector<std::string>>(), ""},
    {"preprocess", NULL, "stop after preprocessing"},
    {"no-inlining", NULL, "disable inlining function calls"},
    {"full-inlining", NULL, "perform full inlining of function calls"},
    {"all-claims", NULL, "keep all claims"},
    {"show-loops", NULL, "show the loops in the program"},
    {"show-claims", NULL, "only show claims"},
    {"show-vcc", NULL, "show the verification conditions"},
    {"document-subgoals", NULL, "generate subgoals documentation"},
    {"no-arch", NULL, "don't set up an architecture"},
    {"no-library", NULL, "disable built-in abstract C library"},
    {"binary", NULL, "read goto program instead of source code"},
    {"little-endian", NULL, "allow little-endian word-byte conversions"},
    {"big-endian", NULL, "allow big-endian word-byte conversions"},
    {"16", NULL, "set width of machine word (default is 64)"},
    {"32", NULL, "set width of machine word (default is 64)"},
    {"64", NULL, "set width of machine word (default is 64)"},
    {"version", NULL, "show current ESBMC version and exit"},
    {"witness-output",
     boost::program_options::value<std::string>(),
     "generate the verification result witness in GraphML format"},
    {"witness-producer", boost::program_options::value<std::string>(), ""},
    {"witness-programfile", boost::program_options::value<std::string>(), ""},
    {"old-frontend",
     NULL,
     "parse source files using our old frontend {deprecated},"},
    {"result-only", NULL, "do not print the counter-example"},
#ifdef _WIN32
    {"i386-macos", NULL, "set MACOS/I386 architecture"},
    {"ppc-macos", NULL, "set PPC/I386 architecture"},
    {"i386-linux", NULL, "set Linux/I386 architecture"},
    {"i386-win32", NULL, "set Windows/I386 architecture (default),"},
#elif __APPLE__
    {"i386-macos", NULL, "set MACOS/I386 architecture (default),"},
    {"ppc-macos", NULL, "set PPC/I386 architecture"},
    {"i386-linux", NULL, "set Linux/I386 architecture"},
    {"i386-win32", NULL, "set Windows/I386 architecture"},
#else
    {"i386-macos", NULL, "set MACOS/I386 architecture"},
    {"ppc-macos", NULL, "set PPC/I386 architecture"},
    {"i386-linux", NULL, "set Linux/I386 architecture (default),"},
    {"i386-win32", NULL, "set Windows/I386 architecture"},
#endif
    {"funsigned-char", NULL, "make \"char\" unsigned by default"},
    {"fms-extensions", NULL, "enable microsoft C extensions"}}},
  {"BMC",
   {{"function",
     boost::program_options::value<std::string>()->value_name("name"),
     "set main function name"},
    {"claim",
     boost::program_options::value<std::vector<int>>()->value_name("nr"),
     "only check specific claim"},
    {"instruction",
     boost::program_options::value<int>()->value_name("nr"),
     "limit the number of instructions executed during symbolic execution"},
    {"unwind",
     boost::program_options::value<int>()->value_name("nr"),
     "unwind nr times"},
    {"unwindset",
     boost::program_options::value<std::string>()->value_name("nr"),
     "unwind given loop nr times"},
    {"no-unwinding-assertions", NULL, "do not generate unwinding assertions"},
    {"partial-loops", NULL, "permit paths with partial loops"},
    {"unroll-loops", NULL, ""},
    {"no-slice", NULL, "do not remove unused equations"},
    {"goto-unwind", NULL, "unroll bounded loops at goto level"},
    {"unlimited-goto-unwind",
     NULL,
     "do not unroll bounded loops at goto level"},
    {"slice-assumes", NULL, "remove unused assume statements"},
    {"extended-try-analysis", NULL, ""},
    {"skip-bmc", NULL, ""}}},
  {"Incremental BMC",
   {{"incremental-bmc", NULL, "incremental loop unwinding verification"},
    {"falsification", NULL, "incremental loop unwinding for bug searching"},
    {"termination",
     NULL,
     "incremental loop unwinding assertion verification"}}},
  {"Solver",
   {{"list-solvers", NULL, "list available solvers and exit"},
    {"boolector", NULL, "use Boolector (default),"},
    {"z3", NULL, "use Z3"},
    {"mathsat", NULL, "use MathSAT"},
    {"cvc", NULL, "use CVC4"},
    {"yices", NULL, "use Yices"},
    {"bitwuzla", NULL, "use Bitwuzla"},
    {"bv", NULL, "use solver with bit-vector arithmetic"},
    {"ir", NULL, "use solver with integer/real arithmetic"},
    {"smtlib", NULL, "use SMT lib format"},
    {"smtlib-solver-prog",

     boost::program_options::value<std::string>(),
     "SMT lib program name"},
    {"output",
     boost::program_options::value<std::string>()->value_name("<filename>"),
     "output VCCs in SMT lib format to given file"},
    {"floatbv",
     NULL,
     "encode floating-point using the SMT floating-point theory(default)"},
    {"fixedbv", NULL, "encode floating-point as fixed bit-vectors"},
    {"fp2bv",
     NULL,
     "encode floating-point as bit-vectors(default for solvers that don't "
     "support the SMT floating-point theory)"},
    {"tuple-node-flattener", NULL, "encode tuples using our tuple to node API"},
    {"tuple-sym-flattener",
     NULL,
     "encode tuples using our tuple to symbol API"},
    {"array-flattener", NULL, "encode arrays using our array API"},
    {"no-return-value-opt",
     NULL,
     "disable return value optimization to compute the stack size"}}},

  {"Incremental SMT",
   {{"smt-during-symex",
     NULL,
     "enable incremental SMT solving {experimental},"},
    {"smt-thread-guard",
     NULL,
     "check the thread guard during thread exploration {experimental},"},
    {"smt-symex-guard",
     NULL,
     "check conditional goto statements during symbolic execution "
     "{experimental},"},
    {"smt-symex-assert",
     NULL,
     "check assertion statements during symbolic execution {experimental},"}}},
  {"Property checking",
   {{"no-assertions", NULL, "ignore assertions"},
    {"no-bounds-check", NULL, "do not do array bounds check"},
    {"no-div-by-zero-check", NULL, "do not do division by zero check"},
    {"no-pointer-check", NULL, "do not do pointer check"},
    {"no-align-check", NULL, "do not check pointer alignment"},
    {"no-pointer-relation-check", NULL, "do not check pointer relations"},
    {"nan-check", NULL, "check floating-point for NaN"},
    {"memory-leak-check", NULL, "enable memory leak check"},
    {"overflow-check", NULL, "enable arithmetic over- and underflow check"},
    {"struct-fields-check",
     NULL,
     "enable over-sized read checks for struct fields"},
    {"deadlock-check",
     NULL,
     "enable global and local deadlock check with mutex"},
    {"data-races-check", NULL, "enable data races check"},
    {"lock-order-check", NULL, "enable for lock acquisition ordering check"},
    {"atomicity-check", NULL, "enable atomicity check at visible assignments"},
    {"stack-limit",
     boost::program_options::value<int>()->default_value(-1)->value_name(
       "bits"),
     "check if stack limit is respected"},
    {"error-label",
     boost::program_options::value<std::string>()->value_name("label"),
     "check if label is unreachable"},
    {"force-malloc-success", NULL, "do not check for malloc/new failure"}}},
  {"k-induction",
   {{"base-case", NULL, "check the base case"},
    {"forward-condition", NULL, "check the forward condition"},
    {"inductive-step", NULL, "check the inductive step"},
    {"k-induction", NULL, "prove by k-induction "},
    {"k-induction-parallel",
     NULL,
     "prove by k-induction, running each step on a separate process"},
    {"k-step",
     boost::program_options::value<int>()->default_value(1)->value_name("nr"),
     "set k increment (default is 1)"},
    {"max-k-step",
     boost::program_options::value<int>()->default_value(50)->value_name("nr"),
     "set max number of iteration (default is 50)"},
    {"show-cex",
     NULL,
     "print the counter-example produced by the inductive step"},
    {"bidirectional", NULL, ""},
    {"unlimited-k-steps", NULL, "set max number of iteration to UINT_MAX"},
    {"max-inductive-step",
     boost::program_options::value<int>()->default_value(-1)->value_name("nr"),
     ""}}},
  {"Scheduling",
   {{"schedule", NULL, "use schedule recording approach"},
    {"round-robin", NULL, "use the round robin scheduling approach"},
    {"time-slice",
     boost::program_options::value<int>()->default_value(1)->value_name("nr"),
     "set the time slice of the round robin algorithm (default is 1) "}}},
  {"Concurrency checking",
   {{"context-bound",
     boost::program_options::value<int>()->default_value(-1)->value_name("nr"),
     "limit number of context switches for each thread"},
    {"state-hashing", NULL, "enable state-hashing, prunes duplicate states"},
    {"no-por", NULL, "do not do partial order reduction"},
    {"all-runs",
     NULL,
     "check all interleavings, even if a bug was already found"}}},
  {"Miscellaneous options",
   {

     {"memlimit",
      boost::program_options::value<std::string>()->value_name("limit"),
      "configure memory limit, of form \"100m\" or \"2g\""},
     {"memstats", NULL, "print memory usage statistics"},
     {"timeout",
      boost::program_options::value<std::string>()->value_name("t"),
      "configure time limit, integer followed by {s,m,h}"},
     {"enable-core-dump", NULL, "do not disable core dump output"},
     {"no-simplify", NULL, "do not simplify any expression"},
     {"no-propagation", NULL, "disable constant propagation"},
     {"interval-analysis",
      NULL,
      "enable interval analysis for integer variables and add assumes to the "
      "program"},
     {"add-symex-value-sets",
      NULL,
      "enable value-set analysis for pointers and add assumes to the "
      "program"}}},

  {"DEBUG options",
   {// Print commit hash for current binary
    {"git-hash", NULL, ""},
    // Check if there is two or more assingments to the same SSA instruction
    {"double-assign-check", NULL, ""},
    // Abort if the program contains a recursion
    {"abort-on-recursion", NULL, ""},
    // Verbosity of message, probably does nothing
    {"verbosity", boost::program_options::value<int>(), ""},
    // --break-at $insnnum will cause ESBMC to execute a trap
    // instruction when it executes the designated GOTO instruction number.
    {"break-at", boost::program_options::value<std::string>(), ""},
    // I added some intrinsics along the line of "__ESBMC_switch_to_thread"
    // that immediately transitioned to a particular thread and didn't allow
    // any other exploration from that point. Useful for constructing an
    // explicit multithreading path
    {"direct-interleavings", NULL, ""},
    // I think this dumps the current stack of all threads on an ileave point.
    // Useful for working out the state of _all_ the threads and how they
    // evolve, also see next flag,
    {"print-stack-traces", NULL, ""},
    // At every ileave point ESBMC stops and asks the user what thread to
    // transition to. Useful again for trying to replicate a particular context
    // switch order, or quickly explore what's reachable.
    {"interactive-ileaves", NULL, ""}}},
  {"end", {{"", NULL, "end of options"}}},
  {"Hidden Options",
   {{"depth", boost::program_options::value<int>(), "instruction"},
    {"explain,h", NULL, ""}}}};
