/*******************************************************************\

Module: C++ Language Type Checking

Author: Daniel Kroening, kroening@cs.cmu.edu

\*******************************************************************/

#include <util/c_qualifiers.h>
#include <cpp/cpp_class_type.h>
#include <cpp/cpp_convert_type.h>
#include <cpp/cpp_exception_id.h>
#include <cpp/cpp_type2name.h>
#include <cpp/cpp_typecheck.h>
#include <clang-cpp-frontend/expr2cpp.h>
#include <util/arith_tools.h>
#include <util/bitvector.h>
#include <util/c_sizeof.h>
#include <util/c_types.h>
#include <util/config.h>
#include <util/expr_util.h>
#include <util/i2string.h>
#include <util/simplify_expr.h>
#include <util/std_expr.h>
#include <util/std_types.h>

bool cpp_typecheckt::find_parent(
  const symbolt &symb,
  const irep_idt &base_name,
  irep_idt &identifier)
{
  forall_irep(bit, symb.type.find("bases").get_sub())
  {
    if(lookup(bit->type().identifier())->name == base_name)
    {
      identifier = bit->type().identifier();
      return true;
    }
  }

  return false;
}

void cpp_typecheckt::typecheck_expr_main(exprt &expr)
{
  if(expr.id() == "cpp-name")
    typecheck_expr_cpp_name(expr, cpp_typecheck_fargst());
  else if(expr.id() == "cpp-this")
    typecheck_expr_this(expr);
  else if(expr.id() == "pointer-to-member")
    convert_pmop(expr);
  else if(expr.id() == "new_object")
  {
  }
  else if(operator_is_overloaded(expr))
  {
  }
  else if(expr.id() == "explicit-typecast")
    typecheck_expr_explicit_typecast(expr);
  else if(expr.id() == "explicit-constructor-call")
    typecheck_expr_explicit_constructor_call(expr);
  else if(expr.id() == "string-constant")
  {
    c_typecheck_baset::typecheck_expr_main(expr);

    // we need to adjust the subtype to 'char'
    assert(expr.type().id() == "array");
    expr.type().subtype().set("#cpp_type", "char");
  }
  else if(expr.is_nil())
  {
    std::cerr << "cpp_typecheckt::typecheck_expr_main got nil" << std::endl;
    abort();
  }
  else if(expr.id() == "__is_base_of")
  {
    // an MS extension
    // http://msdn.microsoft.com/en-us/library/ms177194(v=vs.80).aspx

    typet base = static_cast<const typet &>(expr.find("type_arg1"));
    typet deriv = static_cast<const typet &>(expr.find("type_arg2"));

    typecheck_type(base);
    typecheck_type(deriv);

    follow_symbol(base);
    follow_symbol(deriv);

    if(base.id() != "struct" || deriv.id() != "struct")
      expr.make_false();
    else
    {
      irep_idt base_name = base.get("name");
      const class_typet &class_type = to_class_type(deriv);

      if(class_type.has_base(base_name))
        expr.make_true();
      else
        expr.make_false();
    }
  }
  else if(expr.id() == "msc_uuidof")
  {
    // these appear to have type "struct _GUID"
    // and they are lvalues!
    expr.type() = symbol_typet("tag._GUID");
    follow(expr.type());
    expr.set("#lvalue", true);
  }
  else
    c_typecheck_baset::typecheck_expr_main(expr);
}

void cpp_typecheckt::typecheck_expr_trinary(exprt &expr)
{
  assert(expr.operands().size() == 3);

  implicit_typecast(expr.op0(), bool_typet());

  if(expr.op1().type().id() == "empty" || expr.op1().type().id() == "empty")
  {
    if(expr.op1().cmt_lvalue())
    {
      exprt e1(expr.op1());
      if(!standard_conversion_lvalue_to_rvalue(e1, expr.op1()))
      {
        err_location(e1);
        str << "error: lvalue to rvalue conversion";
        throw 0;
      }
    }

    if(expr.op1().type().id() == "array")
    {
      exprt e1(expr.op1());
      if(!standard_conversion_array_to_pointer(e1, expr.op1()))
      {
        err_location(e1);
        str << "error: array to pointer conversion";
        throw 0;
      }
    }

    if(expr.op1().type().id() == "code")
    {
      exprt e1(expr.op1());
      if(!standard_conversion_function_to_pointer(e1, expr.op1()))
      {
        err_location(e1);
        str << "error: function to pointer conversion";
        throw 0;
      }
    }

    if(expr.op2().cmt_lvalue())
    {
      exprt e2(expr.op2());
      if(!standard_conversion_lvalue_to_rvalue(e2, expr.op2()))
      {
        err_location(e2);
        str << "error: lvalue to rvalue conversion";
        throw 0;
      }
    }

    if(expr.op2().type().id() == "array")
    {
      exprt e2(expr.op2());
      if(!standard_conversion_array_to_pointer(e2, expr.op2()))
      {
        err_location(e2);
        str << "error: array to pointer conversion";
        throw 0;
      }
    }

    if(expr.op2().type().id() == "code")
    {
      exprt e2(expr.op2());
      if(!standard_conversion_function_to_pointer(e2, expr.op2()))
      {
        err_location(expr);
        str << "error: function to pointer conversion";
        throw 0;
      }
    }

    if(
      expr.op1().statement() == "cpp-throw" &&
      expr.op2().statement() != "cpp-throw")
      expr.type() = expr.op2().type();
    else if(
      expr.op2().statement() == "cpp-throw" &&
      expr.op1().statement() != "cpp-throw")
      expr.type() = expr.op1().type();
    else if(
      expr.op1().type().id() == "empty" && expr.op2().type().id() == "empty")
      expr.type() = empty_typet();
    else
    {
      err_location(expr);
      str << "error: bad types for operands";
      throw 0;
    }
    return;
  }

  if(expr.op1().type() == expr.op2().type())
  {
    c_qualifierst qual1, qual2;
    qual1.read(expr.op1().type());
    qual2.read(expr.op2().type());

    if(qual1.is_subset_of(qual2))
      expr.type() = expr.op1().type();
    else
      expr.type() = expr.op2().type();
  }
  else
  {
    exprt e1 = expr.op1();
    exprt e2 = expr.op2();

    if(implicit_conversion_sequence(expr.op1(), expr.op2().type(), e1))
    {
      if(expr.id() == "if")
      {
        if(e2.type().id() != e1.type().id())
        {
          e2.make_typecast(e1.type());
          expr.op2().swap(e2);
        }
        assert(e1.type().id() == e2.type().id());
      }
      else if(implicit_conversion_sequence(expr.op2(), expr.op1().type(), e2))
      {
        err_location(expr);
        str << "error: type is ambigious";
        throw 0;
      }
      expr.type() = e1.type();
      expr.op1().swap(e1);
    }
    else if(implicit_conversion_sequence(expr.op2(), expr.op1().type(), e2))
    {
      expr.type() = e2.type();
      expr.op2().swap(e2);
    }
    else if(
      expr.op1().type().id() == "array" && expr.op2().type().id() == "array" &&
      expr.op1().type().subtype() == expr.op2().type().subtype())
    {
      // array-to-pointer conversion

      index_exprt index1;
      index1.array() = expr.op1();
      index1.index() = from_integer(0, int_type());
      index1.type() = expr.op1().type().subtype();

      index_exprt index2;
      index2.array() = expr.op2();
      index2.index() = from_integer(0, int_type());
      index2.type() = expr.op2().type().subtype();

      address_of_exprt addr1(index1);
      address_of_exprt addr2(index2);

      expr.op1() = addr1;
      expr.op2() = addr2;
      expr.type() = addr1.type();
      return;
    }
    else
    {
      err_location(expr);
      str << "error: types are incompatible.\n"
          << "I got `" << type2cpp(expr.op1().type(), *this) << "' and `"
          << type2cpp(expr.op2().type(), *this) << "'.";
      throw 0;
    }
  }

  if(expr.op1().cmt_lvalue() && expr.op2().cmt_lvalue())
    expr.set("#lvalue", true);
}

void cpp_typecheckt::typecheck_expr_member(exprt &expr)
{
  typecheck_expr_member(expr, cpp_typecheck_fargst());
}

void cpp_typecheckt::typecheck_expr_sizeof(exprt &expr)
{
  // We need to overload, "sizeof-expression" can be mis-parsed
  // as a type.

  if(expr.operands().size() == 0)
  {
    const typet &type = static_cast<const typet &>(expr.find("sizeof-type"));

    if(type.id() == "cpp-name")
    {
      // sizeof(X) may be ambiguous -- X can be either a type or
      // an expression.

      cpp_typecheck_fargst fargs;

      exprt symbol_expr = resolve(
        to_cpp_name(static_cast<const irept &>(type)),
        cpp_typecheck_resolvet::BOTH,
        fargs);

      if(symbol_expr.id() != "type")
      {
        expr.copy_to_operands(symbol_expr);
        expr.remove("sizeof-type");
      }
    }
    else if(type.id() == "array")
    {
      // sizeof(expr[index]) can be parsed as an array type!

      if(type.subtype().id() == "cpp-name")
      {
        cpp_typecheck_fargst fargs;

        exprt symbol_expr = resolve(
          to_cpp_name(static_cast<const irept &>(type.subtype())),
          cpp_typecheck_resolvet::BOTH,
          fargs);

        if(symbol_expr.id() != "type")
        {
          // _NOT_ a type
          index_exprt index_expr(symbol_expr, to_array_type(type).size());
          expr.copy_to_operands(index_expr);
          expr.remove("sizeof-type");
        }
      }
    }
  }

  c_typecheck_baset::typecheck_expr_sizeof(expr);
}

void cpp_typecheckt::typecheck_expr_ptrmember(exprt &expr)
{
  typecheck_expr_ptrmember(expr, cpp_typecheck_fargst());
}

void cpp_typecheckt::typecheck_function_expr(
  exprt &expr,
  const cpp_typecheck_fargst &fargs)
{
  if(expr.id() == "cpp-name")
    typecheck_expr_cpp_name(expr, fargs);
  else if(expr.id() == "member")
  {
    typecheck_expr_operands(expr);
    typecheck_expr_member(expr, fargs);
  }
  else if(expr.id() == "ptrmember")
  {
    typecheck_expr_operands(expr);
    add_implicit_dereference(expr.op0());

    // is operator-> overloaded?
    if(expr.op0().type().id() != "pointer")
    {
      std::string op_name = "operator->";

      // turn this into a function call
      side_effect_expr_function_callt functioncall;
      functioncall.arguments().reserve(expr.operands().size());
      functioncall.location() = expr.location();

      // first do function/operator
      cpp_namet cpp_name;
      cpp_name.get_sub().emplace_back("name");
      cpp_name.get_sub().back().identifier(op_name);
      cpp_name.get_sub().back().add("#location") = expr.location();

      functioncall.function() =
        static_cast<const exprt &>(static_cast<const irept &>(cpp_name));

      // now do the argument
      functioncall.arguments().push_back(expr.op0());
      typecheck_side_effect_function_call(functioncall);

      exprt tmp("already_typechecked");
      tmp.copy_to_operands(functioncall);
      functioncall.swap(tmp);

      expr.op0().swap(functioncall);
      typecheck_function_expr(expr, fargs);
      return;
    }

    typecheck_expr_ptrmember(expr, fargs);
  }
  else
    typecheck_expr(expr);
}

bool cpp_typecheckt::overloadable(const exprt &expr)
{
  // at least one argument must have class or enumerated type

  forall_operands(it, expr)
  {
    typet t = follow(it->type());

    if(is_reference(t))
      t = t.subtype();

    if(t.id() == "struct" || t.id() == "union")
      return true;
  }

  return false;
}

struct operator_entryt
{
  const char *id_name;
  const char *op_name;
} const operators[] = {
  {"+", "+"},         {"-", "-"},           {"*", "*"},
  {"/", "/"},         {"bitnot", "~"},      {"bitand", "&"},
  {"bitor", "|"},     {"bitxor", "^"},      {"not", "!"},
  {"unary-", "-"},    {"and", "&&"},        {"or", "||"},
  {"not", "!"},       {"index", "[]"},      {"=", "=="},
  {"<", "<"},         {"<=", "<="},         {">", ">"},
  {">=", ">="},       {"shl", "<<"},        {"shr", ">>"},
  {"notequal", "!="}, {"dereference", "*"}, {"ptrmember", "->"},
  {nullptr, nullptr}};

bool cpp_typecheckt::operator_is_overloaded(exprt &expr)
{
  // Check argument types first.
  // At least one struct/enum operand is required.

  if(!overloadable(expr))
    return false;
  if(expr.id() == "dereference" && expr.implicit())
    return false;

  assert(expr.operands().size() >= 1);

  if(expr.id() == "explicit-typecast")
  {
    // the cast operator can be overloaded

    typet t = expr.type();
    typecheck_type(t);
    std::string op_name =
      std::string("operator") + "(" + cpp_type2name(t) + ")";

    // turn this into a function call
    side_effect_expr_function_callt functioncall;
    functioncall.arguments().reserve(expr.operands().size());
    functioncall.location() = expr.location();

    cpp_namet cpp_name;
    cpp_name.get_sub().emplace_back("name");
    cpp_name.get_sub().back().identifier(op_name);
    cpp_name.get_sub().back().add("#location") = expr.location();

    // See if the struct decalares the cast operator as a member
    bool found_in_struct = false;
    assert(!expr.operands().empty());
    typet t0(follow(expr.op0().type()));

    if(t0.id() == "struct")
    {
      const struct_typet &struct_type = to_struct_type(t0);

      const struct_typet::componentst &components = struct_type.components();

      for(const auto &component : components)
      {
        if(!component.get_bool("from_base") && component.base_name() == op_name)
        {
          found_in_struct = true;
          break;
        }
      }
    }

    if(!found_in_struct)
      return false;

    {
      exprt member("member");
      member.add("component_cpp_name") = cpp_name;

      exprt tmp("already_typechecked");
      tmp.copy_to_operands(expr.op0());
      member.copy_to_operands(tmp);

      functioncall.function() = member;
    }

    if(expr.operands().size() > 1)
    {
      for(exprt::operandst::const_iterator it = (expr.operands().begin() + 1);
          it != (expr).operands().end();
          it++)
        functioncall.arguments().push_back(*it);
    }

    typecheck_side_effect_function_call(functioncall);

    if(expr.id() == "ptrmember")
    {
      add_implicit_dereference(functioncall);
      exprt tmp("already_typechecked");
      tmp.move_to_operands(functioncall);
      expr.op0().swap(tmp);
      typecheck_expr(expr);
      return true;
    }

    expr.swap(functioncall);
    return true;
  }

  for(const operator_entryt *e = operators; e->id_name != nullptr; e++)
    if(expr.id() == e->id_name)
    {
      std::string op_name = std::string("operator") + e->op_name;

      // first do function/operator
      cpp_namet cpp_name;
      cpp_name.get_sub().emplace_back("name");
      cpp_name.get_sub().back().identifier(op_name);
      cpp_name.get_sub().back().add("#location") = expr.location();

      // turn this into a function call
      side_effect_expr_function_callt functioncall;
      functioncall.arguments().reserve(expr.operands().size());
      functioncall.location() = expr.location();

      // See if the struct decalares the operator as a member
      /*assert(!expr.operands().empty());
      typet t0(follow(expr.op0().type()));*/

      functioncall.function() =
        static_cast<const exprt &>(static_cast<const irept &>(cpp_name));

      // now do arguments
      forall_operands(it, expr)
        functioncall.arguments().push_back(*it);

      typecheck_side_effect_function_call(functioncall);

      if(expr.id() == "ptrmember")
      {
        add_implicit_dereference(functioncall);
        exprt tmp("already_typechecked");
        tmp.move_to_operands(functioncall);
        expr.op0() = tmp;
        typecheck_expr(expr);
        return true;
      }

      expr = functioncall;

      return true;
    }

  return false;
}

void cpp_typecheckt::typecheck_expr_address_of(exprt &expr)
{
  if(expr.operands().size() != 1)
  {
    err_location(expr);
    throw "address_of expects one operand";
  }

  exprt &op = expr.op0();

  if(!op.cmt_lvalue() && expr.type().id() == "code")
  {
    err_location(expr.location());
    str << "expr not an lvalue";
    throw 0;
  }

  if(expr.op0().type().id() == "code")
  {
    // we take the address of the method.
    assert(expr.op0().id() == "member");
    exprt symb = cpp_symbol_expr(*lookup(expr.op0().component_name()));
    exprt address("address_of", typet("pointer"));
    address.copy_to_operands(symb);
    address.type().subtype() = symb.type();
    address.set("#implicit", true);
    expr.op0().swap(address);
  }

  if(expr.op0().id() == "address_of" && expr.op0().implicit())
  {
    // must be the address of a function
    code_typet &code_type = to_code_type(op.type().subtype());

    code_typet::argumentst &args = code_type.arguments();
    if(args.size() > 0 && args[0].cmt_base_name() == "this")
    {
      // it's a pointer to member function
      typet symbol("symbol");
      symbol.set("identifier", code_type.get("#member_name"));
      expr.op0().type().add("to-member") = symbol;

      if(code_type.get_bool("#is_virtual"))
      {
        err_location(expr.location());
        str << "error: pointers to virtual methods"
            << " are currently not implemented";
        throw 0;
      }
    }
  }

  c_typecheck_baset::typecheck_expr_address_of(expr);
}

void cpp_typecheckt::typecheck_expr_throw(exprt &expr)
{
  // these are of type void
  expr.type() = empty_typet();

  assert(expr.operands().size() == 1 || expr.operands().size() == 0);

  if(expr.operands().size() == 1)
  {
    // nothing really to do; one can throw _almost_ anything
    const typet &exception_type = expr.op0().type();

    irep_idt id = follow(exception_type).id();

    if(id == "empty")
    {
      err_location(expr.op0());
      throw "cannot throw void";
    }

    // annotate the relevant exception IDs
    expr.set("exception_list", cpp_exception_list(exception_type, *this));
  }
}

void cpp_typecheckt::typecheck_expr_new(exprt &expr)
{
  // next, find out if we do an array

  if(expr.type().id() == "array")
  {
    // first typecheck subtype
    typecheck_type(expr.type().subtype());

    // typecheck the size
    exprt &size = to_array_type(expr.type()).size();
    typecheck_expr(size);

    bool size_is_unsigned = (size.type().id() == "unsignedbv");
    typet integer_type(size_is_unsigned ? "unsignedbv" : "signedbv");
    integer_type.width(config.ansi_c.int_width);
    implicit_typecast(size, integer_type);

    expr.statement("cpp_new[]");

    // save the size expression
    expr.set("size", to_array_type(expr.type()).size());

    // new actually returns a pointer, not an array
    pointer_typet ptr_type;
    ptr_type.subtype() = expr.type().subtype();
    expr.type().swap(ptr_type);
  }
  else
  {
    // first typecheck type
    typecheck_type(expr.type());

    expr.statement("cpp_new");

    pointer_typet ptr_type;
    ptr_type.subtype().swap(expr.type());
    expr.type().swap(ptr_type);
  }

  exprt object_expr("new_object", expr.type().subtype());
  object_expr.set("#lvalue", true);

  {
    exprt tmp("already_typechecked");
    tmp.move_to_operands(object_expr);
    object_expr.swap(tmp);
  }

  // not yet typechecked-stuff
  exprt &initializer = static_cast<exprt &>(expr.add("initializer"));

  // arrays must not have an initializer
  if(!initializer.operands().empty() && expr.statement() == "cpp_new[]")
  {
    err_location(expr.op0());
    str << "new with array type must not use initializer";
    throw 0;
  }

  exprt code =
    cpp_constructor(expr.find_location(), object_expr, initializer.operands());

  expr.add("initializer").swap(code);

  // we add the size of the object for convenience of the
  // runtime library

  exprt &sizeof_expr = static_cast<exprt &>(expr.add("sizeof"));
  sizeof_expr = c_sizeof(expr.type().subtype(), *this);
  sizeof_expr.c_sizeof_type(expr.type().subtype());
}

void cpp_typecheckt::typecheck_expr_explicit_typecast(exprt &expr)
{
  // these can have 0 or 1 arguments

  if(expr.operands().size() == 0)
  {
    // Default value, e.g., int()
    typecheck_type(expr.type());
    exprt new_expr = gen_zero(expr.type());

    if(new_expr.is_nil())
    {
      err_location(expr);
      str << "no default value for `" << to_string(expr.type()) << "'";
      throw 0;
    }

    new_expr.location() = expr.location();
    expr = new_expr;
  }
  else if(expr.operands().size() == 1)
  {
    // Explicitly given value, e.g., int(1).
    // There is an expr-vs-type ambiguity, as it is possible to write
    // (f)(1), where 'f' is a function symbol and not a type.
    // This also exists with a "comma expression", e.g.,
    // (f)(1, 2, 3)

    if(expr.type().id() == "cpp-name")
    {
      // try to resolve as type
      cpp_typecheck_fargst fargs;

      exprt symbol_expr = resolve(
        to_cpp_name(static_cast<const irept &>(expr.type())),
        cpp_typecheck_resolvet::TYPE,
        fargs,
        false); // fail silently

      if(symbol_expr.id() == "type")
        expr.type() = symbol_expr.type();
      else
      {
        // It's really a function call. Note that multiple arguments
        // become a comma expression, and that these are already typechecked.
        side_effect_expr_function_callt f_call;

        f_call.location() = expr.location();
        f_call.function().swap(expr.type());

        if(expr.op0().id() == "comma")
          f_call.arguments().swap(expr.op0().operands());
        else
          f_call.arguments().push_back(expr.op0());

        typecheck_side_effect_function_call(f_call);

        expr.swap(f_call);
        return;
      }
    }
    else
      typecheck_type(expr.type());

    exprt new_expr;

    if(
      const_typecast(expr.op0(), expr.type(), new_expr) ||
      static_typecast(expr.op0(), expr.type(), new_expr, false) ||
      reinterpret_typecast(expr.op0(), expr.type(), new_expr, false))
    {
      expr = new_expr;
      add_implicit_dereference(expr);
    }
    else
    {
      err_location(expr);
      str << "invalid explicit cast:" << std::endl;
      str << "operand type: `" << to_string(expr.op0().type()) << "'"
          << std::endl;
      str << "casting to: `" << to_string(expr.type()) << "'";
      throw 0;
    }
  }
  else
    throw "explicit typecast expects 0 or 1 operands";
}

void cpp_typecheckt::typecheck_expr_explicit_constructor_call(exprt &expr)
{
  typecheck_type(expr.type());

  if(cpp_is_pod(expr.type()))
  {
    expr.id("explicit-typecast");
    typecheck_expr_main(expr);
  }
  else
  {
    assert(expr.type().id() == "struct");

    typet symb("symbol");
    symb.identifier(expr.type().name());
    symb.location() = expr.location();

    exprt e = expr;
    new_temporary(e.location(), symb, e.operands(), expr);
  }
}

void cpp_typecheckt::typecheck_expr_this(exprt &expr)
{
  if(cpp_scopes.current_scope().class_identifier.empty())
  {
    err_location(expr);
    error("`this' is not allowed here");
    throw 0;
  }

  const exprt &this_expr = cpp_scopes.current_scope().this_expr;
  const locationt location = expr.find_location();

  assert(this_expr.is_not_nil());
  assert(this_expr.type().id() == "pointer");

  expr = this_expr;
  expr.location() = location;
}

void cpp_typecheckt::typecheck_expr_delete(exprt &expr)
{
  if(expr.operands().size() != 1)
    throw "delete expects one operand";

  const irep_idt statement = expr.statement();

  if(statement == "cpp_delete")
  {
  }
  else if(statement == "cpp_delete[]")
  {
  }
  else
    assert(false);

  typet pointer_type = follow(expr.op0().type());

  if(pointer_type.id() != "pointer")
  {
    err_location(expr);
    str << "delete takes a pointer type operand, but got `"
        << to_string(pointer_type) << "'";
    throw 0;
  }

  // remove any const-ness of the argument
  // (which would impair the call to the destructor)
  pointer_type.subtype().remove("#constant");

  // delete expressions are always void
  expr.type() = typet("empty");

  // we provide the right destructor, for the convenience
  // of later stages
  exprt new_object("new_object", pointer_type.subtype());
  new_object.location() = expr.location();
  new_object.set("#lvalue", true);

  already_typechecked(new_object);

  codet destructor_code =
    cpp_destructor(expr.location(), pointer_type.subtype(), new_object);

  // this isn't typechecked yet
  if(destructor_code.is_not_nil())
    typecheck_code(destructor_code);

  expr.set("destructor", destructor_code);
}

void cpp_typecheckt::typecheck_expr_typecast(exprt &)
{
// should not be called
#if 0
  std::cout << "E: " << expr.pretty() << std::endl;
  assert(0);
#endif
}

void cpp_typecheckt::typecheck_expr_member(
  exprt &expr,
  const cpp_typecheck_fargst &fargs)
{
  if(expr.operands().size() != 1)
  {
    err_location(expr);
    str << "error: member operator expects one operand";
    throw 0;
  }

  exprt &op0 = expr.op0();
  add_implicit_dereference(op0);

  // The notation for explicit calls to destructors can be used regardless
  // of whether the type defines a destructor.  This allows you to make such
  // explicit calls without knowing if a destructor is defined for the type.
  // An explicit call to a destructor where none is defined has no effect.

  if(
    expr.find("component_cpp_name").is_not_nil() &&
    to_cpp_name(expr.find("component_cpp_name")).is_destructor() &&
    follow(op0.type()).id() != "struct")
  {
    exprt tmp("cpp_dummy_destructor");
    tmp.location() = expr.location();
    expr.swap(tmp);
    return;
  }

  if(op0.type().id() != "symbol")
  {
    err_location(expr);
    str << "error: member operator requires type symbol "
           "on left hand side but got `"
        << to_string(op0.type()) << "'";
    throw 0;
  }

  typet op_type = op0.type();
  // Follow symbolic types up until the last one.
  while(lookup(op_type.identifier())->type.id() == "symbol")
    op_type = lookup(op_type.identifier())->type;

  const irep_idt &struct_identifier = to_symbol_type(op_type).get_identifier();

  const symbolt &struct_symbol = *lookup(struct_identifier);

  if(
    struct_symbol.type.id() == "incomplete_struct" ||
    struct_symbol.type.id() == "incomplete_union" ||
    struct_symbol.type.id() == "incomplete_class")
  {
    err_location(expr);
    str << "error: member operator got incomplete type "
           "on left hand side";
    throw 0;
  }

  if(struct_symbol.type.id() != "struct" && struct_symbol.type.id() != "union")
  {
    err_location(expr);
    str << "error: member operator requires struct/union type "
           "on left hand side but got `"
        << to_string(struct_symbol.type) << "'";
    throw 0;
  }

  const struct_typet &type = to_struct_type(struct_symbol.type);

  if(expr.find("component_cpp_name").is_not_nil())
  {
    cpp_namet component_cpp_name = to_cpp_name(expr.find("component_cpp_name"));

    // go to the scope of the struct/union
    cpp_save_scopet save_scope(cpp_scopes);
    cpp_scopes.set_scope(struct_identifier);

    // resolve the member name in this scope
    cpp_typecheck_fargst new_fargs(fargs);
    new_fargs.add_object(op0);

    exprt symbol_expr =
      resolve(component_cpp_name, cpp_typecheck_resolvet::VAR, new_fargs);

    if(symbol_expr.id() == "dereference")
    {
      assert(symbol_expr.implicit());
      exprt tmp = symbol_expr.op0();
      symbol_expr.swap(tmp);
    }

    assert(
      symbol_expr.id() == "symbol" || symbol_expr.id() == "member" ||
      symbol_expr.id() == "constant");

    // If it is a symbol or a constant, just return it!
    // Note: the resolver returns a symbol if the member
    // is static or if it is a constructor.

    if(symbol_expr.id() == "symbol")
    {
      if(
        symbol_expr.type().id() == "code" &&
        symbol_expr.type().get("return_type") == "constructor")
      {
        err_location(expr);
        str << "error: member `" << lookup(symbol_expr.identifier())->name
            << "' is a constructor";
        throw 0;
      }

      // it must be a static component
      const struct_typet::componentt pcomp =
        type.get_component(to_symbol_expr(symbol_expr).get_identifier());

      if(pcomp.is_nil())
      {
        err_location(expr);
        str << "error: `" << symbol_expr.identifier()
            << "' is not static member "
            << "of class `" << struct_symbol.name << "'";
        throw 0;
      }

      expr = symbol_expr;
      return;
    }
    if(symbol_expr.id() == "constant")
    {
      expr = symbol_expr;
      return;
    }

    const irep_idt component_name = symbol_expr.component_name();

    expr.remove("component_cpp_name");
    expr.component_name(component_name);
  }

  const irep_idt &component_name = expr.component_name();

  assert(component_name != "");

  exprt component;
  component.make_nil();

  assert(
    follow(expr.op0().type()).id() == "struct" ||
    follow(expr.op0().type()).id() == "union");

  exprt member;

  if(get_component(expr.location(), expr.op0(), component_name, member))
  {
    // because of possible anonymous members
    expr.swap(member);
  }
  else
  {
    err_location(expr);
    str << "error: member `" << component_name << "' of `" << struct_symbol.name
        << "' not found";
    throw 0;
  }

  add_implicit_dereference(expr);

  if(expr.type().id() == "code")
  {
    // Check if the function body has to be typechecked
    symbolt *s = context.find_symbol(component_name);
    assert(s != nullptr);

    symbolt &func_symb = *s;

    if(func_symb.value.id() == "cpp_not_typechecked")
      func_symb.value.set("is_used", true);
  }
}

void cpp_typecheckt::typecheck_expr_ptrmember(
  exprt &expr,
  const cpp_typecheck_fargst &fargs)
{
  assert(expr.id() == "ptrmember");

  if(expr.operands().size() != 1)
  {
    err_location(expr);
    str << "error: ptrmember operator expects one operand";
    throw 0;
  }

  add_implicit_dereference(expr.op0());

  if(expr.op0().type().id() != "pointer")
  {
    err_location(expr);
    str << "error: ptrmember operator requires pointer type "
           "on left hand side, but got `"
        << to_string(expr.op0().type()) << "'";
    throw 0;
  }

  exprt tmp;
  exprt &op = expr.op0();

  op.swap(tmp);

  op.id("dereference");
  op.move_to_operands(tmp);
  op.set("#location", expr.find("#location"));
  typecheck_expr_dereference(op);

  expr.id("member");
  typecheck_expr_member(expr, fargs);
}

void cpp_typecheckt::typecheck_cast_expr(exprt &expr)
{
  side_effect_expr_function_callt e = to_side_effect_expr_function_call(expr);

  if(e.arguments().size() != 1)
  {
    err_location(expr);
    throw "cast expressions expect one operand";
  }

  exprt &f_op = e.function();
  exprt &cast_op = e.arguments().front();

  add_implicit_dereference(cast_op);

  const irep_idt &id = f_op.get_sub().front().identifier();

  if(f_op.get_sub().size() != 2 || f_op.get_sub()[1].id() != "template_args")
  {
    err_location(expr);
    str << id << " expects template argument";
    throw 0;
  }

  irept &template_arguments = f_op.get_sub()[1].add("arguments");

  if(template_arguments.get_sub().size() != 1)
  {
    err_location(expr);
    str << id << " expects one template argument";
    throw 0;
  }

  irept &template_arg = template_arguments.get_sub().front();

  if(template_arg.id() != "type" && template_arg.id() != "ambiguous")
  {
    err_location(expr);
    str << id << " expects a type as template argument";
    throw 0;
  }

  typet &type =
    static_cast<typet &>(template_arguments.get_sub().front().type());

  typecheck_type(type);

  locationt location = expr.location();

  exprt new_expr;
  if(id == "const_cast")
  {
    if(!const_typecast(cast_op, type, new_expr))
    {
      err_location(cast_op);
      str << "type mismatch on const_cast:" << std::endl;
      str << "operand type: `" << to_string(cast_op.type()) << "'" << std::endl;
      str << "cast type: `" << to_string(type) << "'";
      throw 0;
    }
    new_expr.set("cast", "const");
  }
  else if(id == "dynamic_cast")
  {
    if(!dynamic_typecast(cast_op, type, new_expr))
    {
      err_location(cast_op);
      str << "type mismatch on dynamic_cast:" << std::endl;
      str << "operand type: `" << to_string(cast_op.type()) << "'" << std::endl;
      str << "cast type: `" << to_string(type) << "'";
      throw 0;
    }
    new_expr.set("cast", "dynamic");
  }
  else if(id == "reinterpret_cast")
  {
    if(!reinterpret_typecast(cast_op, type, new_expr))
    {
      err_location(cast_op);
      str << "type mismatch on reinterpret_cast:" << std::endl;
      str << "operand type: `" << to_string(cast_op.type()) << "'" << std::endl;
      str << "cast type: `" << to_string(type) << "'";
      throw 0;
    }
    new_expr.set("cast", "reinterpret");
  }
  else if(id == "static_cast")
  {
    if(!static_typecast(cast_op, type, new_expr))
    {
      err_location(cast_op);
      str << "type mismatch on static_cast:" << std::endl;
      str << "operand type: `" << to_string(cast_op.type()) << "'" << std::endl;
      str << "cast type: `" << to_string(type) << "'";
      throw 0;
    }
    new_expr.set("cast", "static");
  }
  else
    assert(false);

  expr.swap(new_expr);
}

void cpp_typecheckt::typecheck_expr_cpp_name(
  exprt &expr,
  const cpp_typecheck_fargst &fargs)
{
  locationt location = to_cpp_name(expr).location();

  for(auto &i : expr.get_sub())
  {
    if(i.id() == "cpp-name")
    {
      typet &type = static_cast<typet &>(i);
      typecheck_type(type);

      std::string tmp = "(" + cpp_type2name(type) + ")";

      typet name("name");
      name.identifier(tmp);
      name.location() = location;

      type = name;
    }
  }

  if(expr.get_sub().size() >= 1 && expr.get_sub().front().id() == "name")
  {
    const irep_idt &id = expr.get_sub().front().identifier();

    if(
      id == "const_cast" || id == "dynamic_cast" || id == "reinterpret_cast" ||
      id == "static_cast")
    {
      expr.id("cast_expression");
      return;
    }
  }

  exprt symbol_expr =
    resolve(to_cpp_name(expr), cpp_typecheck_resolvet::VAR, fargs);

  // if symbol_expr is a type and not an expr, then the type
  // has to be a POD
  assert(symbol_expr.id() != "type" || cpp_is_pod(symbol_expr.type()));

  if(symbol_expr.id() == "member")
  {
    if(symbol_expr.operands().empty() || symbol_expr.op0().is_nil())
    {
      if(symbol_expr.type().get("return_type") != "constructor")
      {
        if(cpp_scopes.current_scope().this_expr.is_nil())
        {
          if(symbol_expr.type().id() != "code")
          {
            err_location(location);
            str << "object missing";
            throw 0;
          }

          // may still be good for address of
        }
        else
        {
          // Try again
          exprt ptrmem("ptrmember");
          ptrmem.operands().push_back(cpp_scopes.current_scope().this_expr);

          ptrmem.add("component_cpp_name") = expr;

          ptrmem.location() = location;
          typecheck_expr_ptrmember(ptrmem, fargs);
          symbol_expr.swap(ptrmem);
        }
      }
    }
  }

  symbol_expr.location() = location;
  expr = symbol_expr;

  if(expr.id() == "symbol")
    typecheck_expr_function_identifier(expr);

  add_implicit_dereference(expr);
}

void cpp_typecheckt::add_implicit_dereference(exprt &expr)
{
  if(is_reference(expr.type()))
  {
    // add implicit dereference
    exprt tmp("dereference", expr.type().subtype());
    tmp.set("#implicit", true);
    tmp.location() = expr.location();
    tmp.move_to_operands(expr);
    tmp.set("#lvalue", true);
    expr.swap(tmp);
  }
}

void cpp_typecheckt::typecheck_expr_typeid(exprt &expr)
{
  // expr.op0() contains the typeid function
  exprt typeid_function = expr.op0();

  // First, let's check if we're getting the function name
  irept component_cpp_name = typeid_function.find("component_cpp_name");

  if(component_cpp_name.get_sub().size() != 1)
  {
    err_location(typeid_function.location());
    str << "only typeid(*).name() is supported\n";
    throw 0;
  }

  irep_idt identifier = component_cpp_name.get_sub()[0].identifier();
  if(identifier != "name")
  {
    err_location(typeid_function.location());
    str << "only typeid(*).name() is supported\n";
    throw 0;
  }

  // Second, let's check the typeid parameter
  exprt function = typeid_function.op0();
  exprt arguments = function.op1().op0();

  if(arguments.get_sub().size()) // It's an object
  {
    typecheck_expr_cpp_name(arguments, cpp_typecheck_fargst());

    // If the object on typeid is a null pointer we must
    // throw a bad_typeid exception
    symbolt pointer_symbol = *lookup(arguments.identifier());

    if(pointer_symbol.value.value() == "NULL")
    {
      // It's NULL :( Let's add a throw bad_typeid

      // Let's create the bad_typeid exception
      irep_idt bad_typeid_identifier = "std::tag.bad_typeid";

      // We must check if the user included typeinfo
      const symbolt *bad_typeid_symbol = lookup(bad_typeid_identifier);
      bool is_included = !bad_typeid_symbol;

      if(is_included)
        throw "Error: must #include <typeinfo> before using typeid";

      // Ok! Let's create the temp object bad_typeid
      exprt bad_typeid;
      bad_typeid.identifier(bad_typeid_identifier);
      bad_typeid.operands().emplace_back("sideeffect");
      bad_typeid.op0().type() = typet("symbol");
      bad_typeid.op0().type().identifier(bad_typeid_identifier);

      // Check throw
      typecheck_expr_throw(bad_typeid);

      // Save on the expression for handling on goto-program
      function.set("exception_list", bad_typeid.find("exception_list"));
    }

    if(arguments.type().id() == "incomplete_array")
    {
      err_location(arguments.location());
      str << "storage size of ‘" << lookup(arguments.identifier())->name;
      str << "’ isn’t known\n";
      throw 0;
    }
  }
  else // Type or index
  {
    if(arguments.id() == "index")
    {
      Forall_operands(it, arguments)
        typecheck_expr_cpp_name(*it, cpp_typecheck_fargst());
    }
    else
    {
      // Typecheck the type
      typet type(arguments.id());
      typecheck_type(type);

      // Create exprt like the ones for not types
      exprt type_symbol("symbol");
      type_symbol.identifier(arguments.id());
      type_symbol.type() = type;
      type_symbol.location() = arguments.location();

      // Swap to the arguments
      arguments.swap(type_symbol);
    }
  }

  // Finally, replace the expr with the correct values
  // and set its return type to const char*

  // Swap back to function
  function.op1().op0().swap(arguments);

  // Swap back to expr
  expr.op0().op0().swap(function);

  // Set return type
  typet char_type(irep_idt("char"));
  typecheck_type(char_type);

  pointer_typet char_pointer_type(char_type);
  expr.type() = char_pointer_type;
}

void cpp_typecheckt::typecheck_side_effect_function_call(
  side_effect_expr_function_callt &expr)
{
  // For virtual functions, it is important to check whether
  // the function name is qualified. If it is qualified, then
  // the call is not virtual.
  bool is_qualified = false;

  if(expr.function().id() == "member" || expr.function().id() == "ptrmember")
  {
    if(expr.function().get("component_cpp_name") == "cpp-name")
    {
      const cpp_namet &cpp_name =
        to_cpp_name(expr.function().find("component_cpp_name"));
      is_qualified = cpp_name.is_qualified();
    }
  }
  else if(expr.function().id() == "cpp-name")
  {
    const cpp_namet &cpp_name = to_cpp_name(expr.function());
    is_qualified = cpp_name.is_qualified();
  }

  // Backup of the original operand
  exprt op0 = expr.function();

  // Check typeid and return, we'll only check its parameters
  if(op0.has_operands())
  {
    if(op0.op0().statement() == "typeid")
    {
      typecheck_expr_typeid(expr);
      return;
    }
  }

  // now do the function -- this has been postponed
  cpp_typecheck_fargst fargs(expr);
  // If the expression is decorated with what the 'this' object is, add it to
  // the fargst record. If it isn't available, name resolution will still work,
  // it just won't take the 'this' argument into account when overloading. (NB:
  // this is bad).
  if(expr.find("#this_expr").is_not_nil())
    fargs.add_object(static_cast<const exprt &>(expr.find("#this_expr")));

  typecheck_function_expr(expr.function(), fargs);

  if(expr.function().id() == "type")
  {
    // otherwise we would have found a constructor
    assert(cpp_is_pod(expr.function().type()));

    if(expr.arguments().size() == 0)
    {
      if(expr.function().type().find("#cpp_type").is_not_nil())
      {
        exprt typecast("explicit-typecast");
        typecast.type().swap(expr.function().type());
        typecast.location() = expr.location();
        typecheck_expr_explicit_typecast(typecast);
        expr.swap(typecast);
      }
      else
      {
        // create temporary object
        exprt tmp_object_expr("sideeffect", expr.op0().type());
        tmp_object_expr.statement("temporary_object");
        tmp_object_expr.set("#lvalue", true);
        tmp_object_expr.set("mode", current_mode);
        tmp_object_expr.location() = expr.location();
        expr.swap(tmp_object_expr);
      }
    }
    else if(expr.arguments().size() == 1)
    {
      exprt typecast("explicit-typecast");
      typecast.type().swap(op0);
      typecast.location() = expr.location();
      typecast.copy_to_operands(expr.arguments().front());
      typecheck_expr_explicit_typecast(typecast);
      expr.swap(typecast);
    }
    else
    {
      err_location(expr.location());
      str << "zero or one argument excpected\n";
      throw 0;
    }

    return;
  }

  if(expr.function().id() == "cast_expression")
  {
    // These are not really function calls,
    // but usually just type adjustments.
    typecheck_cast_expr(expr);
    add_implicit_dereference(expr);
    return;
  }
  if(expr.function().id() == "cpp_dummy_destructor")
  {
    // these don't do anything, e.g., (char*)->~char()
    expr.statement("skip");
    expr.type() = empty_typet();
    return;
  }

  // look at type of function

  follow_symbol(expr.function().type());

  if(expr.function().type().id() == "pointer")
  {
    if(expr.function().type().find("to-member").is_not_nil())
    {
      const exprt &bound =
        static_cast<const exprt &>(expr.function().type().find("#bound"));

      if(bound.is_nil())
      {
        err_location(expr.location());
        str << "pointer-to-member not bound";
        throw 0;
      }

      // add `this'
      assert(bound.type().id() == "pointer");
      expr.arguments().insert(expr.arguments().begin(), bound);

      // we don't need the object anymore
      expr.function().type().remove("#bound");
    }

    // do implicit dereference
    if(
      (expr.function().id() == "implicit_address_of" ||
       expr.function().id() == "address_of") &&
      expr.function().operands().size() == 1)
    {
      exprt tmp;
      tmp.swap(expr.function().op0());
      expr.function().swap(tmp);
    }
    else
    {
      assert(expr.function().type().id() == "pointer");
      exprt tmp("dereference", expr.function().type().subtype());
      tmp.location() = expr.op0().location();
      tmp.move_to_operands(expr.function());
      expr.function().swap(tmp);
    }

    if(expr.function().type().id() != "code")
    {
      err_location(expr.op0());
      throw "expecting code as argument";
    }
  }
  else if(expr.function().type().id() == "code")
  {
    if(expr.function().type().get_bool("#is_virtual") && !is_qualified)
    {
      exprt vtptr_member;
      if(op0.id() == "member" || op0.id() == "ptrmember")
      {
        vtptr_member.id(op0.id());
        vtptr_member.move_to_operands(op0.op0());
      }
      else
      {
        vtptr_member.id("ptrmember");
        exprt this_expr("cpp-this");
        vtptr_member.move_to_operands(this_expr);
      }

      // get the virtual table
      typet this_type =
        to_code_type(expr.function().type()).arguments().front().type();
      irep_idt vtable_name =
        this_type.subtype().identifier().as_string() + "::@vtable_pointer";

      const struct_typet &vt_struct =
        to_struct_type(follow(this_type.subtype()));

      const struct_typet::componentt &vt_compo =
        vt_struct.get_component(vtable_name);

      assert(vt_compo.is_not_nil());

      vtptr_member.component_name(vtable_name);

      // look for the right entry
      irep_idt vtentry_component_name =
        vt_compo.type().subtype().identifier().as_string() +
        "::" + expr.function().type().get("#virtual_name").as_string();

      exprt vtentry_member("ptrmember");
      vtentry_member.copy_to_operands(vtptr_member);
      vtentry_member.component_name(vtentry_component_name);
      typecheck_expr(vtentry_member);

      assert(vtentry_member.type().id() == "pointer");

      {
        exprt tmp("dereference", vtentry_member.type().subtype());
        tmp.location() = expr.op0().location();
        tmp.move_to_operands(vtentry_member);
        vtentry_member.swap(tmp);
      }

      // Typcheck the expresssion as if it was not virtual
      // (add the this pointer)

      expr.type() = to_code_type(expr.function().type()).return_type();

      typecheck_method_application(expr);

      // Let's make the call virtual
      expr.function().swap(vtentry_member);

      typecheck_function_call_arguments(expr);
      add_implicit_dereference(expr);
      return;
    }
  }
  else if(expr.function().type().id() == "struct")
  {
    irept name("name");
    name.identifier("operator()");
    name.set("#location", expr.location());

    cpp_namet cppname;
    cppname.get_sub().push_back(name);

    exprt member("member");
    member.add("component_cpp_name") = cppname;

    member.move_to_operands(op0);

    expr.function().swap(member);
    typecheck_side_effect_function_call(expr);

    return;
  }
  else
  {
    err_location(expr.function());
    str << "function call expects function or function "
        << "pointer as argument, but got `" << to_string(expr.op0().type())
        << "'";
    throw 0;
  }

  expr.type() = to_code_type(expr.function().type()).return_type();

  if(expr.type().id() == "constructor")
  {
    assert(expr.function().id() == "symbol");

    const code_typet::argumentst &arguments =
      to_code_type(expr.function().type()).arguments();

    assert(arguments.size() >= 1);

    const typet &this_type = arguments[0].type();

    // change type from 'constructor' to object type
    expr.type() = this_type.subtype();

    // create temporary object
    exprt tmp_object_expr("sideeffect", this_type.subtype());
    tmp_object_expr.statement("temporary_object");
    tmp_object_expr.set("#lvalue", true);
    tmp_object_expr.set("mode", current_mode);
    tmp_object_expr.location() = expr.location();

    exprt member;

    exprt new_object("new_object", tmp_object_expr.type());
    new_object.set("#lvalue", true);

    assert(follow(tmp_object_expr.type()).id() == "struct");

    get_component(
      expr.location(), new_object, expr.function().identifier(), member);

    // special case for the initialization of parents
    if(member.get_bool("#not_accessible"))
    {
      assert(id2string(member.get("#access")) != "");
      tmp_object_expr.set("#not_accessible", true);
      tmp_object_expr.set("#access", member.get("#access"));
    }

    expr.function().swap(member);

    typecheck_method_application(expr);
    typecheck_function_call_arguments(expr);

    codet new_code("expression");
    new_code.copy_to_operands(expr);

    tmp_object_expr.add("initializer") = new_code;
    expr.swap(tmp_object_expr);
    return;
  }

  assert(expr.operands().size() == 2);

  if(expr.function().id() == "member")
    typecheck_method_application(expr);
  else
  {
    // for the object of a method call,
    // we are willing to add an "address_of"
    // for the sake of operator overloading

    const irept::subt &arguments = expr.function().type().arguments().get_sub();

    if(
      arguments.size() >= 1 && arguments.front().cmt_base_name() == "this" &&
      expr.arguments().size() >= 1)
    {
      const exprt &argument = static_cast<const exprt &>(arguments.front());

      exprt &operand = expr.op1();
      assert(argument.type().id() == "pointer");

      if(
        operand.type().id() != "pointer" &&
        operand.type() == argument.type().subtype())
      {
        exprt tmp("address_of", typet("pointer"));
        tmp.type().subtype() = operand.type();
        tmp.location() = operand.location();
        tmp.move_to_operands(operand);
        operand.swap(tmp);
      }
    }
  }

  assert(expr.operands().size() == 2);

  typecheck_function_call_arguments(expr);

  assert(expr.operands().size() == 2);

  add_implicit_dereference(expr);

  // we will deal with some 'special' functions here
  do_special_functions(expr);
}

void cpp_typecheckt::typecheck_function_call_arguments(
  side_effect_expr_function_callt &expr)
{
  exprt &f_op = expr.function();
  const code_typet &code_type = to_code_type(f_op.type());
  const code_typet::argumentst &arguments = code_type.arguments();

  // do default arguments

  if(arguments.size() > expr.arguments().size())
  {
    unsigned i = expr.arguments().size();

    for(; i < arguments.size(); i++)
    {
      if(!arguments[i].has_default_value())
        break;

      const exprt &value = arguments[i].default_value();
      expr.arguments().push_back(value);
    }
  }

  for(unsigned i = 0; i < arguments.size(); i++)
  {
    if(arguments[i].get_bool("#call_by_value"))
    {
      assert(is_reference(arguments[i].type()));

      if(expr.arguments()[i].id() != "temporary_object")
      {
        // create a temporary for the parameter

        exprt arg("already_typechecked");
        arg.copy_to_operands(expr.arguments()[i]);

        exprt temporary;
        new_temporary(
          expr.arguments()[i].location(),
          arguments[i].type().subtype(),
          arg,
          temporary);
        expr.arguments()[i].swap(temporary);
      }
    }
  }

  c_typecheck_baset::typecheck_function_call_arguments(expr);
}

void cpp_typecheckt::typecheck_expr_side_effect(side_effect_exprt &expr)
{
  const irep_idt &statement = expr.statement();

  if(statement == "cpp_new" || statement == "cpp_new[]")
  {
    typecheck_expr_new(expr);
  }
  else if(statement == "cpp_delete" || statement == "cpp_delete[]")
  {
    typecheck_expr_delete(expr);
  }
  else if(
    statement == "preincrement" || statement == "predecrement" ||
    statement == "postincrement" || statement == "postdecrement")
  {
    typecheck_side_effect_increment(expr);
  }
  else if(statement == "cpp-throw")
  {
    typecheck_expr_throw(expr);
  }
  else
    c_typecheck_baset::typecheck_expr_side_effect(expr);
}

void cpp_typecheckt::typecheck_method_application(
  side_effect_expr_function_callt &expr)
{
  assert(expr.operands().size() == 2);

  assert(expr.function().id() == "member");
  assert(expr.function().operands().size() == 1);

  // turn e.f(...) into xx::f(e, ...)

  exprt member_expr;
  member_expr.swap(expr.function());

  const symbolt &symbol = *lookup(member_expr.component_name());

  // build new function expression
  exprt new_function(cpp_symbol_expr(symbol));
  new_function.location() = member_expr.location();
  expr.function().swap(new_function);

  if(!expr.function().type().get_bool("#is_static"))
  {
    const code_typet &func_type = to_code_type(symbol.type);
    typet this_type = func_type.arguments().front().type();

    // Special case. Make it a reference.
    assert(this_type.id() == "pointer");
    this_type.set("#reference", true);
    this_type.set("#this", true);

    if(expr.arguments().size() == func_type.arguments().size())
    {
      implicit_typecast(expr.arguments().front(), this_type);
      assert(is_reference(expr.arguments().front().type()));
      expr.arguments().front().type().remove("#reference");
    }
    else
    {
      exprt this_arg = member_expr.op0();
      implicit_typecast(this_arg, this_type);
      assert(is_reference(this_arg.type()));
      this_arg.type().remove("#reference");
      expr.arguments().insert(expr.arguments().begin(), this_arg);
    }
  }

  if(
    symbol.value.id() == "cpp_not_typechecked" &&
    !symbol.value.get_bool("is_used"))
  {
    context.find_symbol(symbol.id)->value.set("is_used", true);
  }
}

void cpp_typecheckt::typecheck_side_effect_assignment(exprt &expr)
{
  if(expr.operands().size() != 2)
    throw "assignment side-effect expected to have two operands";

  typet type0 = expr.op0().type();

  if(is_reference(type0))
    type0 = type0.subtype();

  if(cpp_is_pod(type0))
  {
    // for structs we use the 'implicit assignment operator',
    // and therefore, it is allowed to assign to a rvalue struct.
    if(follow(type0).id() == "struct")
      expr.op0().set("#lvalue", true);

    c_typecheck_baset::typecheck_side_effect_assignment(expr);

    // Note that in C++ (as opposed to C), the assignment yields
    // an lvalue!
    expr.set("#lvalue", true);

    // If it was a type cast, we need to update the symbol
    if(expr.operands().size() > 0 && expr.op1().id() == "typecast")
    {
      if(expr.op0().identifier() != "")
      {
        symbolt &symbol = *context.find_symbol(expr.op0().identifier());
        if(expr.op1().has_operands())
        {
          exprt &initializer =
            static_cast<exprt &>(expr.op1().op0().add("initializer"));

          if(initializer.has_operands())
          {
            symbol.value = initializer.op0();
          }
        }
      }
      //Array
      else if(expr.op0().type().subtype().identifier() != "")
      {
        const symbolt *symbol = lookup(expr.op0().op0().identifier());
        bool is_included = !symbol;

        if(expr.op1().has_operands())
        {
          exprt &initializer =
            static_cast<exprt &>(expr.op1().op0().add("initializer"));

          if(initializer.has_operands())
          {
            if(!is_included) //find
            {
              symbolt &symbol_temp =
                *context.find_symbol(expr.op0().op0().identifier());
              symbol_temp.value.id("array");
              symbol_temp.value.operands().push_back(initializer.op0());
            }
          }
        }
      }
    }
    return;
  }

  // It's a non-POD.
  // Turn into an operator call

  std::string strop = "operator";

  const irep_idt statement = expr.statement();

  if(statement == "assign")
    strop += "=";
  else if(statement == "assign_shl")
    strop += "<<=";
  else if(statement == "assign_shr")
    strop += ">>=";
  else if(statement == "assign+")
    strop += "+=";
  else if(statement == "assign-")
    strop += "-=";
  else if(statement == "assign*")
    strop += "*=";
  else if(statement == "assign_div")
    strop += "/=";
  else if(statement == "assign_bitand")
    strop += "&=";
  else if(statement == "assign_bitor")
    strop += "|=";
  else if(statement == "assign_bitxor")
    strop += "^=";
  else
  {
    err_location(expr);
    str << "bad assignment operator `" << statement << "'";
    throw 0;
  }

  cpp_namet cpp_name;
  cpp_name.get_sub().emplace_back("name");
  cpp_name.get_sub().front().identifier(strop);
  cpp_name.get_sub().front().set("#location", expr.location());

  // expr.op0() is already typechecked
  exprt already_typechecked("already_typechecked");
  already_typechecked.move_to_operands(expr.op0());

  exprt member("member");
  member.set("component_cpp_name", cpp_name);
  member.move_to_operands(already_typechecked);

  side_effect_expr_function_callt new_expr;
  new_expr.function().swap(member);
  new_expr.arguments().push_back(expr.op1());
  new_expr.location() = expr.location();

  typecheck_side_effect_function_call(new_expr);

  expr.swap(new_expr);
}

void cpp_typecheckt::typecheck_side_effect_increment(side_effect_exprt &expr)
{
  if(expr.operands().size() != 1)
    throw std::string("statement ") + id2string(expr.get_statement()) +
      " expected to have one operand";

  add_implicit_dereference(expr.op0());

  typet tmp_type = follow(expr.op0().type());

  if(is_number(tmp_type) || tmp_type.id() == "pointer")
  {
    // standard stuff
    c_typecheck_baset::typecheck_expr_side_effect(expr);
    return;
  }

  // Turn into an operator call

  std::string str_op = "operator";
  bool post = false;

  if(expr.statement() == "preincrement")
    str_op += "++";
  else if(expr.statement() == "predecrement")
    str_op += "--";
  else if(expr.statement() == "postincrement")
  {
    str_op += "++";
    post = true;
  }
  else if(expr.statement() == "postdecrement")
  {
    str_op += "--";
    post = true;
  }
  else
  {
    err_location(expr);
    str << "bad assignment operator `" << expr.get_statement() << "'";
    throw 0;
  }

  cpp_namet cpp_name;
  cpp_name.get_sub().emplace_back("name");
  cpp_name.get_sub().front().identifier(str_op);
  cpp_name.get_sub().front().set("#location", expr.location());

  exprt already_typechecked("already_typechecked");
  already_typechecked.move_to_operands(expr.op0());

  exprt member("member");
  member.set("component_cpp_name", cpp_name);
  member.move_to_operands(already_typechecked);

  side_effect_expr_function_callt new_expr;
  new_expr.function().swap(member);
  new_expr.location() = expr.location();

  // the odd C++ way to denote the post-inc/dec operator
  if(post)
    new_expr.arguments().push_back(from_integer(BigInt(0), int_type()));

  typecheck_side_effect_function_call(new_expr);
  expr.swap(new_expr);
}

void cpp_typecheckt::typecheck_expr_dereference(exprt &expr)
{
  if(expr.operands().size() != 1)
  {
    err_location(expr);
    str << "unary operator * expects one operand";
    throw 0;
  }

  exprt &op = expr.op0();
  const typet op_type = follow(op.type());

  if(op_type.id() == "pointer" && op_type.find("to-member").is_not_nil())
  {
    err_location(expr);
    str << "pointer-to-member must use "
        << "the .* or ->* operators";
    throw 0;
  }

  c_typecheck_baset::typecheck_expr_dereference(expr);
}

void cpp_typecheckt::convert_pmop(exprt &expr)
{
  assert(expr.id() == "pointer-to-member");
  assert(expr.operands().size() == 2);

  if(
    expr.op1().type().id() != "pointer" ||
    expr.op1().type().find("to-member").is_nil())
  {
    err_location(expr.location());
    str << "pointer-to-member expected\n";
    throw 0;
  }

  typet t0 = expr.op0().type().id() == "pointer" ? expr.op0().type().subtype()
                                                 : expr.op0().type();

  typet t1((const typet &)expr.op1().type().find("to-member"));

  t0 = follow(t0);
  t1 = follow(t1);

  if(t0.id() != "struct")
  {
    err_location(expr.location());
    str << "pointer-to-member type error";
    throw 0;
  }

  const struct_typet &from_struct = to_struct_type(t0);
  const struct_typet &to_struct = to_struct_type(t1);

  if(!subtype_typecast(from_struct, to_struct))
  {
    err_location(expr.location());
    str << "pointer-to-member type error";
    throw 0;
  }

  if(expr.op1().type().subtype().id() != "code")
  {
    err_location(expr);
    str << "pointers to data member are not supported";
    throw 0;
  }

  typecheck_expr_main(expr.op1());

  if(expr.op0().type().id() != "pointer")
  {
    if(expr.op0().id() == "dereference")
    {
      exprt tmp = expr.op0().op0();
      expr.op0().swap(tmp);
    }
    else
    {
      assert(expr.op0().cmt_lvalue());
      exprt address_of("address_of", typet("pointer"));
      address_of.copy_to_operands(expr.op0());
      address_of.type().subtype() = address_of.op0().type();
      expr.op0().swap(address_of);
    }
  }

  exprt tmp(expr.op1());
  tmp.type().set("#bound", expr.op0());
  expr.swap(tmp);
}

void cpp_typecheckt::typecheck_expr_function_identifier(exprt &expr)
{
  if(expr.id() == "symbol")
  {
    // Check if the function body has to be typechecked
    symbolt *s = context.find_symbol(expr.identifier());
    assert(s != nullptr);

    symbolt &func_symb = *s;

    if(func_symb.value.id() == "cpp_not_typechecked")
      func_symb.value.set("is_used", true);
  }

  c_typecheck_baset::typecheck_expr_function_identifier(expr);
}

void cpp_typecheckt::typecheck_expr(exprt &expr)
{
  bool override_constantness = expr.get_bool("#override_constantness");

  c_typecheck_baset::typecheck_expr(expr);

  if(override_constantness)
    expr.type().set("#constant", false);
}

void cpp_typecheckt::typecheck_expr_binary_arithmetic(exprt &expr)
{
  if(expr.operands().size() != 2)
  {
    err_location(expr);
    str << "operator `" << expr.id() << "' expects two operands";
    throw 0;
  }

  add_implicit_dereference(expr.op0());
  add_implicit_dereference(expr.op1());

  c_typecheck_baset::typecheck_expr_binary_arithmetic(expr);
}

void cpp_typecheckt::typecheck_expr_index(exprt &expr)
{
  c_typecheck_baset::typecheck_expr_index(expr);
}

void cpp_typecheckt::typecheck_expr_comma(exprt &expr)
{
  if(expr.operands().size() != 2)
  {
    err_location(expr);
    str << "comma operator expects two operands";
    throw 0;
  }

  if(follow(expr.op0().type()).id() == "struct")
  {
    // TODO: check if the comma operator has been overloaded!
  }

  c_typecheck_baset::typecheck_expr_comma(expr);
}

void cpp_typecheckt::typecheck_expr_rel(exprt &expr)
{
  if(expr.operands().size() != 2)
  {
    err_location(expr);
    str << "operator `" << expr.id() << "' expects two operands";
    throw 0;
  }

  c_typecheck_baset::typecheck_expr_rel(expr);
}
