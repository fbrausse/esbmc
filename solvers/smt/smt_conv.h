#ifndef _ESBMC_PROP_SMT_SMT_CONV_H_
#define _ESBMC_PROP_SMT_SMT_CONV_H_

#include <stdint.h>

#include <irep2.h>
#include <message.h>
#include <namespace.h>
#include <threeval.h>

#include <util/pointer_offset_size.h>

#include <solvers/prop/pointer_logic.h>
#include <solvers/prop/literal.h>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

/** @file smt_conv.h
 *  SMT conversion tools and utilities.
 *  smt_convt is the base class for everything that attempts to convert the
 *  contents of an SSA program into something else, generally SMT or SAT based.
 *
 *  The class itself does various accounting and structuring of the conversion,
 *  however the challenge is that as we convert the SSA program into anything
 *  else, we must deal with the fact that expressions in ESBMC are somewhat
 *  bespoke, and don't follow any particular formalism or logic. Therefore
 *  a lot of translation has to occur to reduce it to the desired logic, a
 *  process that Kroening refers to in CBMC as 'Flattenning'.
 *
 *  The conceptual data flow is that an SSA program held by
 *  symex_target_equationt is converted into a series of boolean propositions
 *  in some kind of solver context, the handle to which are objects of class 
 *  smt_ast. These are then asserted as appropriate (or conjoined or
 *  disjuncted), after which the solver may be asked whether the formula is
 *  or not. If it is, the value of symbols in the formula may be retrieved
 *  from the solver.
 *
 *  To do that, the user must allocate a solver converter object, which extends
 *  the class smt_convt. Current, create_solver_factory will do this, in the
 *  factory-pattern manner (ish). Each solver converter implements all the
 *  abstract methods of smt_convt. When handed an expression to convert,
 *  smt_convt deconstructs it into a series of function applications, which it
 *  creates by calling various abstract methods implemented by the converter
 *  (in particular mk_func_app).
 *
 *  The actual function applications are in smt_ast objects. Following the
 *  SMTLIB definition, these are basically a term.
 *
 *  In no particular order, the following expression translation problems exist
 *  and are solved at various layers:
 *
 *  For all solvers, the following problems are flattenned:
 *    * The C memory address space
 *    * Representation of pointer types
 *    * Casts
 *    * Byte operations on objects (extract/update)
 *    * FixedBV representation of floats
 *    * Unions -> something else
 *
 *  While these problems are supported by some SMT solvers, but are flattened
 *  in others (as SMT doesn't support these):
 *    * Bitvector integer overflow detection
 *    * Tuple representation (and arrays of them)
 *
 *  SAT solvers have the following aspects flattened:
 *    * Arrays (using Kroenings array decision procedure)
 *    * First order logic bitvector calculations to boolean formulas
 *    * Boolean formulas to CNF
 *
 *  If you find yourself having to make the SMT translation translate more than
 *  these things, ask yourself whether what you're doing is better handled at
 *  a different layer, such as symbolic execution. A nonexhaustive list of these
 *  include:
 *    * Anything involving pointer dereferencing at all
 *    * Anything that considers the control flow guard at any point
 *    * Pointer liveness or dynamic allocation consideration
 *
 *  @see smt_convt
 *  @see symex_target_equationt
 *  @see create_solver_factory
 *  @see smt_convt::mk_func_app
 */

/** Identifier for SMT sort kinds
 *  Each different kind of sort (i.e. arrays, bv's, bools, etc) gets its own
 *  identifier. To be able to describe multiple kinds at the same time, they
 *  take binary values, so that they can be used as bits in an integer. */
enum smt_sort_kind {
  SMT_SORT_INT = 1,
  SMT_SORT_REAL = 2,
  SMT_SORT_BV = 4,
  SMT_SORT_ARRAY = 8,
  SMT_SORT_BOOL = 16,
  SMT_SORT_STRUCT = 32,
  SMT_SORT_UNION = 64, // Contencious
};

#define SMT_SORT_ALLINTS (SMT_SORT_INT | SMT_SORT_REAL | SMT_SORT_BV)

/** Identifiers for SMT functions.
 *  Each SMT function gets a unique identifier, representing its interpretation
 *  when applied to some arguments. This can be used to describe a function
 *  application when joined with some arguments. Initial values such as
 *  terminal functions (i.e. bool, int, symbol literals) shouldn't normally
 *  be encountered and instead converted to an smt_ast before use. The
 *  'HACKS' function represents some kind of special case, according to where
 *  it is encountered; the same for 'INVALID'.
 *
 *  @see smt_convt::convert_terminal
 *  @see smt_convt::convert_ast
 */
enum smt_func_kind {
  // Terminals
  SMT_FUNC_HACKS = 0, // indicate the solver /has/ to use the temp expr.
  SMT_FUNC_INVALID = 1, // For conversion lookup table only
  SMT_FUNC_INT = 2,
  SMT_FUNC_BOOL,
  SMT_FUNC_BVINT,
  SMT_FUNC_REAL,
  SMT_FUNC_SYMBOL,

  // Nonterminals
  SMT_FUNC_ADD,
  SMT_FUNC_BVADD,
  SMT_FUNC_SUB,
  SMT_FUNC_BVSUB,
  SMT_FUNC_MUL,
  SMT_FUNC_BVMUL,
  SMT_FUNC_DIV,
  SMT_FUNC_BVUDIV,
  SMT_FUNC_BVSDIV,
  SMT_FUNC_MOD,
  SMT_FUNC_BVSMOD,
  SMT_FUNC_BVUMOD,
  SMT_FUNC_SHL,
  SMT_FUNC_BVSHL,
  SMT_FUNC_BVASHR,
  SMT_FUNC_NEG,
  SMT_FUNC_BVNEG,
  SMT_FUNC_BVLSHR,
  SMT_FUNC_BVNOT,
  SMT_FUNC_BVNXOR,
  SMT_FUNC_BVNOR,
  SMT_FUNC_BVNAND,
  SMT_FUNC_BVXOR,
  SMT_FUNC_BVOR,
  SMT_FUNC_BVAND,

  // Logic
  SMT_FUNC_IMPLIES,
  SMT_FUNC_XOR,
  SMT_FUNC_OR,
  SMT_FUNC_AND,
  SMT_FUNC_NOT,

  // Comparisons
  SMT_FUNC_LT,
  SMT_FUNC_BVSLT,
  SMT_FUNC_BVULT,
  SMT_FUNC_GT,
  SMT_FUNC_BVSGT,
  SMT_FUNC_BVUGT,
  SMT_FUNC_LTE,
  SMT_FUNC_BVSLTE,
  SMT_FUNC_BVULTE,
  SMT_FUNC_GTE,
  SMT_FUNC_BVSGTE,
  SMT_FUNC_BVUGTE,

  SMT_FUNC_EQ,
  SMT_FUNC_NOTEQ,

  SMT_FUNC_ITE,

  SMT_FUNC_STORE,
  SMT_FUNC_SELECT,

  SMT_FUNC_CONCAT,
  SMT_FUNC_EXTRACT, // Not for going through mk app due to sillyness.

  SMT_FUNC_INT2REAL,
  SMT_FUNC_REAL2INT,
  SMT_FUNC_POW,
  SMT_FUNC_IS_INT,
};

/** A class for storing an SMT sort.
 *  This class abstractly represents an SMT sort: solver converter classes are
 *  expected to extend this and add fields that store their solvers
 *  representation of the sort. Then, this base class is used as a handle
 *  through the rest of the SMT conversion code.
 *
 *  Only a few piece of sort information are used to make conversion decisions,
 *  and are thus actually stored in the sort object itself.
 *  @see smt_ast
 */
class smt_sort {
public:
  /** Identifies what /kind/ of sort this is.
   *  The specific sort itself may be parameterised with widths and domains,
   *  for example. */
  smt_sort_kind id;
  /** Data size of the sort.
   *  For bitvectors this is the bit width, for arrays the range BV bit width.
   *  For everything else, undefined */
  unsigned long data_width;
  /** BV Width of array domain. For everything else, undefined */
  unsigned long domain_width;

  smt_sort(smt_sort_kind i) : id(i), data_width(0), domain_width(0) { }
  smt_sort(smt_sort_kind i, unsigned long width)
    : id(i), data_width(width), domain_width(0) { }
  smt_sort(smt_sort_kind i, unsigned long rwidth, unsigned long domwidth)
    : id(i), data_width(rwidth), domain_width(domwidth) { }

  virtual ~smt_sort() { }

  /** Deprecated array domain width accessor */
  virtual unsigned long get_domain_width(void) const {
    return domain_width;
  }
  /** Deprecated array range width accessor */
  virtual unsigned long get_range_width(void) const {
    return data_width;
  }
};

/** Storage for flattened tuple sorts.
 *  When flattening tuples (and arrays of them) down to SMT, we need to store
 *  additional type data. This sort is used in tuple code to record that data.
 *  @see smt_tuple.cpp */
class tuple_smt_sort : public smt_sort
{
public:
  /** Actual type (struct or array of structs) of the tuple that's been
   * flattened */
  const type2tc thetype;
  /** Domain width of tuple arrays. Out of date, needs to be merged with
   *  smt_sort's record of this. */
  unsigned long domain_width;

  tuple_smt_sort(const type2tc &type)
    : smt_sort(SMT_SORT_STRUCT), thetype(type), domain_width(0)
  {
  }

  tuple_smt_sort(const type2tc &type, unsigned long dom_width)
    : smt_sort(SMT_SORT_STRUCT), thetype(type), domain_width(dom_width)
  {
  }

  virtual ~tuple_smt_sort() { }

  virtual unsigned long get_domain_width(void) const {
    return domain_width;
  }
};

#define is_tuple_ast_type(x) (is_structure_type(x) || is_pointer_type(x))
#define is_tuple_array_ast_type(x) (is_array_type(x) && (is_structure_type(to_array_type(x).subtype) || is_pointer_type(to_array_type(x).subtype)))

/** Storage of an SMT function application.
 *  This class represents a single SMT function app, abstractly. Solver
 *  converter classes must extend this and add whatever fields are necessary
 *  to represent a function application in the solver they support. A converted
 *  expression becomes an SMT function application; that is then handed around
 *  the rest of the SMT conversion code as an smt_ast.
 *
 *  While an expression becomes an smt_ast, the inverse is not true, and a
 *  single expression may in fact become many smt_asts in various places. See
 *  smt_convt for more detail on how conversion occurs.
 *
 *  The function arguments, and the actual function application itself are all
 *  abstract and dealt with by the solver converter class. Only the sort needs
 *  to be available for us to make conversion decisions.
 *  @see smt_convt
 *  @see smt_sort
 */
class smt_ast {
public:
  /** The sort of this function application. */
  const smt_sort *sort;

  smt_ast(const smt_sort *s) : sort(s) { }
  virtual ~smt_ast() { }
};

/** Function app representing a tuple sorted value.
 *  This AST represents any kind of SMT function that results in something of
 *  a tuple sort. As documented in smt_tuple.c, the result of any kind of
 *  tuple operation that gets flattened is a symbol prefix, which is what this
 *  ast actually stores.
 *
 *  This AST should only be used in smt_tuple.c, if you're using it elsewhere
 *  think very hard about what you're trying to do. Its creation should also
 *  only occur if there is no tuple support in the solver being used, and a
 *  tuple creating method has been called.
 *
 *  @see smt_tuple.c */
class tuple_smt_ast : public smt_ast {
public:
  /** Primary constructor.
   *  @param s The sort of the tuple, of type tuple_smt_sort.
   *  @param _name The symbol prefix of the variables representing this tuples
   *               value. */
  tuple_smt_ast (const smt_sort *s, const std::string &_name) : smt_ast(s),
            name(_name) { }
  virtual ~tuple_smt_ast() { }

  /** The symbol prefix of the variables representing this tuples value, as a
   *  string (i.e., no associated type). */
  const std::string name;
};

/** The base SMT-conversion class/interface.
 *  smt_convt handles a number of decisions that must be made when
 *  deconstructing ESBMC expressions down into SMT representation. See
 *  smt_conv.h for more high level documentation of this.
 *
 *  The basic flow is thus: a class that can create SMT formula in some solver
 *  subclasses smt_convt, implementing abstract methods, in particular
 *  mk_func_app. The rest of ESBMC then calls convert with an expression, and
 *  this class deconstructs it into a series of applications, as documented by
 *  the smt_func_kind enumeration. These are then created via mk_func_app or
 *  some more specific method calls. Boolean sorted ASTs are then asserted
 *  into the solver context.
 *
 *  The exact lifetime of smt asts here is currently undefined, unfortunately,
 *  although smt_convt posesses a cache, so they generally have a reference
 *  in there. This will probably be fixed in the future.
 *
 *  In theory this class supports pushing and popping of solver contexts,
 *  although of course that depends too on the subclass supporting it. However,
 *  this hasn't really been tested since everything here was rewritten from
 *  The Old Way, so don't trust it.
 *
 *  While mk_func_app is supposed to be the primary interface to making SMT
 *  function applications, in some cases we want to introduce some
 *  abstractions, and this becomes unweildy. Thus, tuple and array operations
 *  are performed via virtual function calls. By default, array operations are
 *  then passed through to mk_func_app, while tuples are decomposed into sets
 *  of variables which are then created through mk_func_app. If this isn't
 *  what a solver wants to happen, it can override this and handle that itself.
 *  The idea is that, in the manner of metaSMT, you can then compose a series
 *  of subclasses that perform the desired amount of flattening, and then work
 *  from there. (Some of this is still WIP though).
 *
 *  NB: the whole smt_asts-are-const situation needs to be resolved too.
 *
 *  @see smt_conv.h
 *  @see smt_func_kind */
class smt_convt : public messaget
{
public:
  /** Shorthand for a vector of smt_ast's */
  typedef std::vector<const smt_ast *> ast_vec;

  /** Primary constructor. After construction, smt_post_init must be called
   *  before the object is used as a solver converter.
   *
   *  @param enable_cache Whether or not to store a map of exprs to smt_ast's
   *         in a cache, and return the cached smt_ast if we attempt to convert
   *         an expr a second time.
   *  @param int_encoding Whether nor not we should use QF_AUFLIRA or QF_AUFBV.
   *  @param _ns Namespace for looking up the type of certain symbols.
   *  @param is_cpp Flag indicating whether memory modelling arrays have c:: or
   *         cpp:: prefix to their symbols.
   *  @param tuple_support True if the underlying solver has native tuple
   *         support.
   *  @param no_bools_in_arrays Whether or not the solver supports having
   *         arrays with booleans as the range, which isn't strictly permitted
   *         by SMT, but is by C.
   *  @param can_init_inf_arrs Whether the solver can efficiently initialize
   *         infinite arrays. If it can, the convert_array_of method is used
   *         to create them. If not, a free array is used, and when we fiddle
   *         with pointer tracking modelling arrays we assert that the elements
   *         we use were initialized to a particular value. Ugly, but works on
   *         various solvers. */
  smt_convt(bool enable_cache, bool int_encoding, const namespacet &_ns,
            bool is_cpp, bool tuple_support, bool no_bools_in_arrays,
            bool can_init_inf_arrs);
  ~smt_convt();

  /** Post-constructor setup method. We must create various pieces of memory
   *  model data for tracking, however can't do it from the constructor because
   *  the solver converter itself won't have been initialized itself at that
   *  point. So, once it's ready, the solver converter should call this from
   *  it's constructor. */
  void smt_post_init(void);

  // The API that we provide to the rest of the world:
  /** @{
   *  @name External API to smt_convt. */

  /** Result of a call to dec_solve. Either sat, unsat, or error. SMTLIB is
   *  historic case that needs to go. */
  typedef enum { P_SATISFIABLE, P_UNSATISFIABLE, P_ERROR, P_SMTLIB } resultt;

  /** Push one context on the SMT assertion stack. */
  virtual void push_ctx(void);
  /** Pop one context on the SMT assertion stack. */
  virtual void pop_ctx(void);

  /** Main interface to SMT conversion.
   *  Takes one expression, and converts it into the underlying SMT solver,
   *  returning a single smt_ast that represents the converted expressions
   *  value. The lifetime of the returned pointer is currently undefined.
   *
   *  @param expr The expression to convert into the SMT solver
   *  @return The resulting handle to the SMT value. */
  const smt_ast *convert_ast(const expr2tc &expr);

  /** Make an n-ary 'or' function application.
   *  Takes a vector of smt_ast's, all boolean sorted, and creates a single
   *  'or' function app over all the smt_ast's.
   *  @param v The vector of converted boolean expressions to be 'or''d.
   *  @return The smt_ast handle to the 'or' func app. */
  virtual const smt_ast *make_disjunct(const ast_vec &v);

  /** Make an n-ary 'and' function application.
   *  Takes a vector of smt_ast's, all boolean sorted, and creates a single
   *  'and' function app over all the smt_ast's.
   *  @param v The vector of converted boolean expressions to be 'and''d.
   *  @return The smt_ast handle to the 'and' func app. */
  virtual const smt_ast *make_conjunct(const ast_vec &v);

  /** Create the inverse of an smt_ast. Equivalent to a 'not' operation.
   *  @param a The ast to invert. Must be boolean sorted.
   *  @return The inverted piece of AST. */
  const smt_ast *invert_ast(const smt_ast *a);

  /** Create an ipmlication between two smt_ast's. 
   *  @param a The ast that implies the truth of the other. Boolean.
   *  @param b The ast whos truth is implied. Boolean.
   *  @return The resulting piece of AST. */
  const smt_ast *imply_ast(const smt_ast *a, const smt_ast *b);

  /** Assert the truth of an ast. Equivalent to the 'assert' directive in the
   *  SMTLIB language, this informs the solver that in the satisfying
   *  assignment it attempts to produce, the formula corresponding to the
   *  smt_ast argument must evaluate to true.
   *  @param a A handle to the formula that must be true. */
  virtual void assert_ast(const smt_ast *a) = 0;

  /** Solve the formula given to the solver. The solver will attempt to produce
   *  a satisfying assignment for all of the variables / symbols used in the
   *  formula, where all the asserted sub-formula are true. Results are either
   *  unsat (the formula is inconsistent), sat (an assignment exists), or that
   *  an error occurred.
   *  @return Result code of the call to the solver. */
  virtual resultt dec_solve() = 0;

  /** Fetch a satisfying assignment from the solver. If a previous call to
   *  dec_solve returned satisfiable, then the solver has a set of assignments
   *  to symbols / variables used in the formula. This method retrieves the
   *  value of a symbol, and formats it into an ESBMC expression.
   *  @param expr Variable to get the value of. Must be a symbol expression.
   *  @return Explicit assigned value of expr in the solver. May be nil, in
   *          which case the solver did not assign a value to it for some
   *          reason. */
  virtual expr2tc get(const expr2tc &expr);

  /** Solver name fetcher. Returns a string naming the solver being used, and
   *  potentially it's version, if available.
   *  @return The name of the solver this smt_convt uses. */
  virtual const std::string solver_text()=0;

  /** Fetch the value of a boolean sorted smt_ast. (The 'l' is for literal, and
   *  is historic). Returns a three valued result, of true, false, or
   *  unassigned.
   *  @param a The boolean sorted ast to fetch the value of.
   *  @return A three-valued return val, of the assignment to a. */
  virtual tvt l_get(const smt_ast *a)=0;

  /** @} */

  /** @{
   *  @name Internal conversion API between smt_convt and solver converter */

  /** Create an SMT function application. Using the provided information,
   *  the solver converter should create a function application in the solver
   *  being used, then wrap it in an smt_ast, and return it. If the desired
   *  function application is not supported by the solver, print an error and
   *  abort.
   *
   *  @param s The resulting sort of the func app we are creating.
   *  @param k The kind of function application to create.
   *  @param args Array of function apps to use as arguments to this one.
   *  @param numargs The number of elements in args. Should be consistent with
   *         the function kind k.
   *  @return The resulting function application, wrapped in an smt_ast. */
  virtual smt_ast *mk_func_app(const smt_sort *s, smt_func_kind k,
                               const smt_ast * const *args,
                               unsigned int numargs) = 0;

  /** Create an SMT sort. The sort kind k indicates what kind of sort to create,
   *  and the parameters of the sort are passed in as varargs. Briefly, these
   *  arguments are:
   *  * Bools: None
   *  * Int's: None
   *  * BV's:  Width as a machine integer, and a bool that's true if it's signed
   *  * Arrays: Two pointers to smt_sort's: the domain sort, and the range sort
   *
   *  Structs and unions use @ref mk_struct_sort and @ref mk_union_sort.
   *
   *  @param k The kind of SMT sort that will be created.
   *  @return The smt_sort wrapper for the sort. Lifetime currently undefined */
  virtual smt_sort *mk_sort(const smt_sort_kind k, ...) = 0;

  /** Create an integer smt_ast. That is, an integer in QF_AUFLIRA, rather than
   *  a bitvector.
   *  @param theint BigInt representation of the number to create.
   *  @param sign Whether this integer is considered signed or not.
   *  @return The newly created terminal smt_ast of this integer. */
  virtual smt_ast *mk_smt_int(const mp_integer &theint, bool sign) = 0;

  /** Create a real in a smt_ast.
   *  @param str String representation of the real, to be parsed by the solver.
   *         Tends to be one integer divided ('/') by another. After inspecting
   *         all other options, there are none that are good, this is a
   *         legitimate use of strings.
   *  @return The newly created terminal smt_ast of this real. */
  virtual smt_ast *mk_smt_real(const std::string &str) = 0;

  /** Create a bitvector.
   *  @param theint Integer representation of the bitvector. Any excess bits
   *         in the stored integer should be ignored.
   *  @param sign Whether this bitvector is to be considered signed or not.
   *  @param w Width, in bits, of the bitvector to create.
   *  @return The newly created terminal smt_ast of this bitvector. */
  virtual smt_ast *mk_smt_bvint(const mp_integer &theint, bool sign,
                                unsigned int w) = 0;

  /** Create a boolean.
   *  @param val Whether to create a true or false boolean.
   *  @return The newly created terminal smt_ast of this boolean. */
  virtual smt_ast *mk_smt_bool(bool val) = 0;

  /** Create a symbol / variable. These correspond to renamed SSA variables in
   *  the SSA program, although any other names can be used too, so long as they
   *  don't conflict with anything else.
   *  @param name Textual name of the symbol to create.
   *  @param s The sort of the symbol we're creating.
   *  @param The newly created terminal smt_ast of this symbol. */
  virtual smt_ast *mk_smt_symbol(const std::string &name, const smt_sort *s) =0;

  /** Create a sort representing a struct. i.e., a tuple. Ideally this should
   *  actually be part of the overridden tuple api, but due to history it isn't
   *  yet. If solvers don't support tuples, implement this to abort.
   *  @param type The struct type to create a tuple representation of.
   *  @return The tuple representation of the type, wrapped in an smt_sort. */
  virtual smt_sort *mk_struct_sort(const type2tc &type) = 0;

  // XXX XXX XXX -- turn this into a formulation on top of structs.

  /** Create a sort representing a union. i.e., a tuple. Ideally this should
   *  actually be part of the overridden tuple api, but due to history it isn't
   *  yet. If solvers don't support tuples, implement this to abort.
   *  @param type The union type to create a tuple representation of.
   *  @return The tuple representation of the type, wrapped in an smt_sort. */
  virtual smt_sort *mk_union_sort(const type2tc &type) = 0;

  /** Create an 'extract' func app. Due to the fact that we can't currently
   *  encode integer constants as function arguments without serious faff,
   *  this can't be performed via the medium of mk_func_app. Hence, this api
   *  call.
   *  @param a The source piece of ast to extract a value from.
   *  @param high The topmost bit to select from the source, down to low.
   *  @param low The lowest bit to select from the source.
   *  @param s The sort of the resulting piece of ast. */
  virtual smt_ast *mk_extract(const smt_ast *a, unsigned int high,
                              unsigned int low, const smt_sort *s) = 0;

  virtual expr2tc get_bool(const smt_ast *a) = 0;
  virtual expr2tc get_bv(const type2tc &t, const smt_ast *a) = 0;
  virtual expr2tc get_array_elem(const smt_ast *array, uint64_t index,
                                 const smt_sort *sort) = 0;

  /** @} */

  /** @{
   *  @name Tuple solver-converter API. */
  virtual smt_ast *tuple_create(const expr2tc &structdef);
  virtual smt_ast *tuple_fresh(const smt_sort *s);
  virtual smt_ast *tuple_project(const smt_ast *a, const smt_sort *s,
                                 unsigned int field);
  virtual const smt_ast *tuple_update(const smt_ast *a, unsigned int field,
                                      const expr2tc &val);
  virtual const smt_ast *tuple_equality(const smt_ast *a, const smt_ast *b);
  virtual const smt_ast *tuple_ite(const expr2tc &cond, const expr2tc &trueval,
                             const expr2tc &false_val, const type2tc &sort);

  virtual const smt_ast *tuple_array_create(const type2tc &array_type,
                                            const smt_ast **input_args,
                                            bool const_array,
                                            const smt_sort *domain);
  virtual const smt_ast *tuple_array_select(const smt_ast *a, const smt_sort *s,
                                      const expr2tc &field);
  virtual const smt_ast *tuple_array_update(const smt_ast *a,
                                      const expr2tc &field,
                                      const smt_ast *val, const smt_sort *s);
  virtual const smt_ast *tuple_array_equality(const smt_ast *a, const smt_ast *b);
  virtual const smt_ast *tuple_array_ite(const expr2tc &cond,
                                         const expr2tc &trueval,
                                         const expr2tc &false_val);
  virtual const smt_ast *tuple_array_of(const expr2tc &init_value,
                                        unsigned long domain_width);

  /** @} */

  /** @{
   *  @name Integer overflow solver-converter API. */
  virtual const smt_ast *overflow_arith(const expr2tc &expr);
  virtual smt_ast *overflow_cast(const expr2tc &expr);
  virtual const smt_ast *overflow_neg(const expr2tc &expr);

  /** @} */

  /** @{
   *  @name Array operations solver-converter API. */

  virtual const smt_ast *convert_array_index(const expr2tc &expr,
                                             const smt_sort *ressort);
  virtual const smt_ast *convert_array_store(const expr2tc &expr,
                                             const smt_sort *ressort);

  virtual const smt_ast *mk_select(const expr2tc &array, const expr2tc &idx,
                                   const smt_sort *ressort);
  virtual const smt_ast *mk_store(const expr2tc &array, const expr2tc &idx,
                                  const expr2tc &value,
                                  const smt_sort *ressort);

  virtual const smt_ast *convert_array_of(const expr2tc &init_val,
                                          unsigned long domain_width);

  virtual const smt_ast *convert_array_equality(const expr2tc &a,
                                                const expr2tc &b);

  /** @} */

  /** @{
   *  @name Internal foo. */

  virtual void set_to(const expr2tc &expr, bool value);

  virtual smt_ast *mk_fresh(const smt_sort *s, const std::string &tag);
  std::string mk_fresh_name(const std::string &tag);

  smt_sort *convert_sort(const type2tc &type);
  smt_ast *convert_terminal(const expr2tc &expr);
  const smt_ast *convert_pointer_arith(const expr2tc &expr, const type2tc &t);
  const smt_ast *convert_ptr_cmp(const expr2tc &expr, const expr2tc &expr2,
                                 const expr2tc &templ_expr);
  const smt_ast *convert_addr_of(const expr2tc &expr);
  const smt_ast *convert_member(const expr2tc &expr, const smt_ast *src);
  const smt_ast *convert_identifier_pointer(const expr2tc &expr,
                                            std::string sym);
  const smt_ast *convert_sign_ext(const smt_ast *a, const smt_sort *s,
                                  unsigned int topbit, unsigned int topwidth);
  const smt_ast *convert_zero_ext(const smt_ast *a, const smt_sort *s,
                                  unsigned int topwidth);
  const smt_ast *convert_is_nan(const expr2tc &expr, const smt_ast *oper);
  const smt_ast *convert_byte_extract(const expr2tc &expr);
  const smt_ast *convert_byte_update(const expr2tc &expr);
  void assert_expr(const expr2tc &e);
  const smt_ast *array_create(const expr2tc &expr);
  const smt_ast *tuple_array_create_despatch(const expr2tc &expr,
                                             const smt_sort *domain);
  smt_ast *mk_tuple_symbol(const expr2tc &expr);
  smt_ast *mk_tuple_array_symbol(const expr2tc &expr);
  void tuple_create_rec(const std::string &name, const type2tc &structtype,
                        const smt_ast **inputargs);
  void tuple_ite_rec(const expr2tc &result, const expr2tc &cond,
                     const expr2tc &true_val, const expr2tc &false_val);
  void tuple_array_select_rec(const tuple_smt_ast *ta, const type2tc &subtype,
                              const tuple_smt_ast *result, const expr2tc &field,
                              const expr2tc &arr_width);
  void tuple_array_update_rec(const tuple_smt_ast *ta, const tuple_smt_ast *val,
                              const expr2tc &idx, const tuple_smt_ast *res,
                              const expr2tc &arr_width,
                              const type2tc &subtype);
  const smt_ast * tuple_array_equality_rec(const tuple_smt_ast *a,
                                           const tuple_smt_ast *b,
                                           const expr2tc &arr_width,
                                           const type2tc &subtype);
  void tuple_array_ite_rec(const expr2tc &true_val, const expr2tc &false_val,
                           const expr2tc &cond, const type2tc &type,
                           const type2tc &dom_sort,
                           const expr2tc &res);
  expr2tc tuple_get(const expr2tc &expr);
  expr2tc tuple_array_get(const expr2tc &expr);
  expr2tc tuple_project_sym(const smt_ast *a, unsigned int f, bool dot = false);
  expr2tc tuple_project_sym(const expr2tc &a, unsigned int f, bool dot = false);

  void init_addr_space_array(void);
  void bump_addrspace_array(unsigned int idx, const expr2tc &val);
  std::string get_cur_addrspace_ident(void);
  void finalize_pointer_chain(unsigned int obj_num);

  const smt_ast *convert_typecast_bool(const typecast2t &cast);
  const smt_ast *convert_typecast_fixedbv_nonint(const expr2tc &cast);
  const smt_ast *convert_typecast_to_ints(const typecast2t &cast);
  const smt_ast *convert_typecast_to_ptr(const typecast2t &cast);
  const smt_ast *convert_typecast_from_ptr(const typecast2t &cast);
  const smt_ast *convert_typecast_struct(const typecast2t &cast);
  const smt_ast *convert_typecast(const expr2tc &expr);
  const smt_ast *round_real_to_int(const smt_ast *a);
  const smt_ast *round_fixedbv_to_int(const smt_ast *a, unsigned int width,
                                      unsigned int towidth);

  const struct_union_data &get_type_def(const type2tc &type);
  expr2tc force_expr_to_tuple_sym(const expr2tc &expr);

  const smt_ast *make_bool_bit(const smt_ast *a);
  const smt_ast *make_bit_bool(const smt_ast *a);

  expr2tc fix_array_idx(const expr2tc &idx, const type2tc &array_type);
  unsigned long size_to_bit_width(unsigned long sz);
  unsigned long calculate_array_domain_width(const array_type2t &arr);
  const smt_sort *make_array_domain_sort(const array_type2t &arr);
  type2tc make_array_domain_sort_exp(const array_type2t &arr);
  expr2tc twiddle_index_width(const expr2tc &expr, const type2tc &type);
  type2tc flatten_array_type(const type2tc &type);
  expr2tc array_domain_to_width(const type2tc &type);

  expr2tc decompose_select_chain(const expr2tc &expr, expr2tc &base);
  expr2tc decompose_store_chain(const expr2tc &expr, expr2tc &base);

  const smt_ast *convert_array_of_prep(const expr2tc &expr);
  const smt_ast *pointer_array_of(const expr2tc &init_val,
                                  unsigned long array_width);

  std::string get_fixed_point(const unsigned width, std::string value) const;

  // Ours:
  expr2tc get_array(const smt_ast *array, const type2tc &t);

  /** @} */

  // Types

  // Types for union map.
  struct union_var_mapt {
    std::string ident;
    unsigned int idx;
    unsigned int level;
  };

  typedef boost::multi_index_container<
    union_var_mapt,
    boost::multi_index::indexed_by<
      boost::multi_index::hashed_unique<
        BOOST_MULTI_INDEX_MEMBER(union_var_mapt, std::string, ident)
      >,
      boost::multi_index::ordered_non_unique<
        BOOST_MULTI_INDEX_MEMBER(union_var_mapt, unsigned int, level),
        std::greater<unsigned int>
      >
    >
  > union_varst;

  // Type for (optional) AST cache

  struct smt_cache_entryt {
    const expr2tc val;
    const smt_ast *ast;
    unsigned int level;
  };

  typedef boost::multi_index_container<
    smt_cache_entryt,
    boost::multi_index::indexed_by<
      boost::multi_index::hashed_unique<
        BOOST_MULTI_INDEX_MEMBER(smt_cache_entryt, const expr2tc, val)
      >,
      boost::multi_index::ordered_non_unique<
        BOOST_MULTI_INDEX_MEMBER(smt_cache_entryt, unsigned int, level),
        std::greater<unsigned int>
      >
    >
  > smt_cachet;

  struct expr_op_convert {
    smt_func_kind int_mode_func;
    smt_func_kind bv_mode_func_signed;
    smt_func_kind bv_mode_func_unsigned;
    unsigned int args;
    unsigned long permitted_sorts;
  };

  // Members
  unsigned int ctx_level;

  union_varst union_vars;
  smt_cachet smt_cache;
  std::list<pointer_logict> pointer_logic;
  type2tc pointer_struct;
  const struct_type2t *pointer_type_data; // ptr of pointer_struct
  type2tc machine_int;
  type2tc machine_uint;
  type2tc machine_ptr;
  const smt_sort *machine_int_sort;
  const smt_sort *machine_uint_sort;
  bool caching;
  bool int_encoding;
  const namespacet &ns;
  bool tuple_support;
  bool no_bools_in_arrays;
  bool can_init_unbounded_arrs;
  std::string dyn_info_arr_name;

  std::map<std::string, unsigned int> fresh_map;

  std::list<unsigned int> addr_space_sym_num;
  type2tc addr_space_type;
  const struct_type2t *addr_space_type_data;
  type2tc addr_space_arr_type;
  std::list<std::map<unsigned, unsigned> > addr_space_data;

  static const expr_op_convert smt_convert_table[expr2t::end_expr_id];
  static const std::string smt_func_name_table[expr2t::end_expr_id];
};

#endif /* _ESBMC_PROP_SMT_SMT_CONV_H_ */
