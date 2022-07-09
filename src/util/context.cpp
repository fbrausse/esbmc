/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <util/context.h>
#include <util/message/default_message.h>
#include <util/message/format.h>

bool contextt::add(const symbolt &symbol)
{
  auto [it,ins] = symbols.emplace(symbol.id, symbol);
  if(!ins)
    return true;

  symbol_base_map.emplace(symbol.name, symbol.id);

  ordered_symbols.push_back(&it->second);
  return false;
}

bool contextt::move(symbolt &symbol, symbolt *&new_symbol)
{
  symbolt tmp;
  auto [it,ins] = symbols.emplace(symbol.id, tmp);

  if(!ins)
  {
    new_symbol = &it->second;
    return true;
  }

  symbol_base_map.emplace(symbol.name, symbol.id);

  ordered_symbols.push_back(&it->second);

  it->second.swap(symbol);
  new_symbol = &it->second;
  return false;
}

void contextt::dump() const
{
  msg.debug("\nSymbols:");
  // Do assignments based on "value".
  foreach_operand([](const symbolt &s) { s.dump(); });
}

symbolt *contextt::find_symbol(irep_idt name)
{
  auto it = symbols.find(name);
  if(it != symbols.end())
    return &it->second;
  return nullptr;
}

const symbolt *contextt::find_symbol(irep_idt name) const
{
  auto it = symbols.find(name);
  if(it != symbols.end())
    return &it->second;
  return nullptr;
}

void contextt::erase_symbol(irep_idt name)
{
  symbolst::iterator it = symbols.find(name);
  if(it == symbols.end())
  {
    msg.error("Couldn't find symbol to erase");
    abort();
  }

  ordered_symbols.erase(
    std::remove_if(
      ordered_symbols.begin(),
      ordered_symbols.end(),
      [&name](const symbolt *s) { return s->id == name; }),
    ordered_symbols.end());
  symbols.erase(it);
}

symbolt *contextt::move_symbol_to_context(symbolt &symbol)
{
  symbolt *s = find_symbol(symbol.id);
  if(s == nullptr)
  {
    if(move(symbol, s))
    {
      msg.error(fmt::format(
        "Couldn't add symbol {} to symbol table\n{}", symbol.name, symbol));
      abort();
    }
  }
  else
  {
    // types that are code means functions
    if(s->type.is_code())
    {
      if(symbol.value.is_not_nil() && !s->value.is_not_nil())
        s->swap(symbol);
    }
    else if(s->is_type)
    {
      if(symbol.type.is_not_nil() && !s->type.is_not_nil())
        s->swap(symbol);
    }
  }
  return s;
}
