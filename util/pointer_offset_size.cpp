/*******************************************************************\

Module: Pointer Logic

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <assert.h>

#include <expr.h>
#include <arith_tools.h>
#include <std_types.h>
#include <ansi-c/c_types.h>

#include "pointer_offset_size.h"

mp_integer member_offset(
  const struct_typet &type,
  const irep_idt &member)
{
  const struct_typet::componentst &components=type.components();

  mp_integer result=0;
  unsigned bit_field_bits=0;

  for(struct_typet::componentst::const_iterator
      it=components.begin();
      it!=components.end();
      it++)
  {
    if(it->get_name()==member) break;
    if(it->get_bool("#is_bit_field"))
    {
      bit_field_bits+=binary2integer(it->type().get("width").as_string(), 2).to_long();
    }
    else
    {
      if(bit_field_bits!=0)
      {
        result+=bit_field_bits/8;
        bit_field_bits=0;
      }

      const typet &subtype=it->type();
      mp_integer sub_size=pointer_offset_size(subtype);
      if(sub_size==-1) return -1; // give up
      result+=sub_size;
    }
  }

  return result;
}

mp_integer member_offset(
  const struct_type2t &type,
  const irep_idt &member)
{
  mp_integer result=0;
  unsigned bit_field_bits=0, idx = 0;

  forall_types(it, type.members) {
    if (type.member_names[idx] == member.as_string())
      break;

    // XXXjmorse - just assume we break horribly on bitfields.
#if 0
    if(it->get_bool("#is_bit_field"))
    {
      bit_field_bits+=binary2integer(it->type().get("width").as_string(), 2).to_long();
    }
#endif

    mp_integer sub_size=pointer_offset_size(**it);
    if(sub_size==-1) return -1; // give up
    result+=sub_size;
  }

  return result;
}

mp_integer pointer_offset_size(const typet &type)
{
  if(type.is_array())
  {
    mp_integer sub=pointer_offset_size(type.subtype());
  
    // get size
    const exprt &size=(const exprt &)type.size_irep();

    // constant?
    mp_integer i;
    
    if(to_integer(size, i))
      return mp_integer(1); // we cannot distinguish the elements
    
    return sub*i;
  }
  else if(type.id()=="struct" ||
          type.id()=="union")
  {
    const irept::subt &components=type.components().get_sub();
    
    mp_integer result=1; // for the struct itself

    forall_irep(it, components)
    {
      const typet &subtype=it->type();
      mp_integer sub_size=pointer_offset_size(subtype);
      result+=sub_size;
    }
    
    return result;
  }
  else
    return mp_integer(1);
}

mp_integer pointer_offset_size(const type2t &type)
{
  if(type.type_id == type2t::array_id)
  {
    const array_type2t &ref = dynamic_cast<const array_type2t&>(type);
    mp_integer sub=pointer_offset_size(*ref.subtype.get());

    // get size
    if (ref.array_size->expr_id != expr2t::constant_int_id)
      // Follow previous behaviour
      return mp_integer(1);

    const constant_int2t &sizeref = static_cast<const constant_int2t &>
                                               (*ref.array_size.get());
    return sub * sizeref.constant_value;
  }
  else if(type.type_id == type2t::struct_id ||
          type.type_id == type2t::union_id)
  {
    const struct_union_data &data_ref =
      static_cast<const struct_union_data &>(type);
    const std::vector<type2tc> &members = data_ref.get_structure_members();

    mp_integer result=1; // for the struct itself

    forall_types(it, members) {
      result += pointer_offset_size(**it);
    }

    return result;
  }
  else
    return mp_integer(1);
#if 0
  mp_integer bees(type.get_width());
  bees /= 8; // bits to bytes.
  return bees;
#endif
}

expr2tc
compute_pointer_offset(const namespacet &ns, const expr2tc &expr)
{
  if (is_symbol2t(expr))
    return expr2tc(new constant_int2t(uint_type2(), BigInt(0)));
  else if (is_index2t(expr))
  {
    const index2t &index = to_index2t(expr);
    mp_integer sub_size;
    if (is_array_type(index.source_value->type)) {
      const array_type2t &arr_type = to_array_type(index.source_value->type);
      sub_size = pointer_offset_size(*arr_type.subtype.get());
    } else if (is_string_type(index.source_value->type)) {
      sub_size = 8;
    } else {
      std::cerr << "Unexpected index type in computer_pointer_offset";
      std::cerr << std::endl;
      abort();
    }

    expr2tc result;
    if (is_constant_int2t(index.index)) {
      const constant_int2t &index_val = to_constant_int2t(index.index);
      result = expr2tc(new constant_int2t(uint_type2(),
                                        sub_size * index_val.constant_value));
    } else {
      // Non constant, create multiply.
      expr2tc tmp_size(new constant_int2t(uint_type2(), sub_size));
      result = expr2tc(new mul2t(uint_type2(), tmp_size, index.index));
    }

    return result;
  }
  else if (is_member2t(expr))
  {
    const member2t &memb = to_member2t(expr);

    mp_integer result;
    if (is_struct_type(expr->type)) {
      const struct_type2t &type = to_struct_type(expr->type);
      result = member_offset(type, memb.member);
    } else {
      result = 0; // Union offsets are always 0.
    }

    return expr2tc(new constant_int2t(uint_type2(), result));
  }
  else
  {
    std::cerr << "compute_pointer_offset, unexpected irep:" << std::endl;
    std::cerr << expr->pretty() << std::endl;
    abort();
  }
}
