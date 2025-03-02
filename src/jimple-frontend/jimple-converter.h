//
// Created by rafaelsamenezes on 23/09/2021.
//

#ifndef ESBMC_JIMPLE_CONVERTER_H
#define ESBMC_JIMPLE_CONVERTER_H

#include <jimple-frontend/jimple-language.h>
class jimple_converter
{
public:
  jimple_converter(contextt &_context, jimple_file &_ASTs, const messaget &msg)
    : context(_context), AST(_ASTs), msg(msg)
  {
  }
  bool convert();

protected:
  contextt &context;
  const jimple_file &AST;
  const messaget &msg;
};

#endif //ESBMC_JIMPLE_CONVERTER_H