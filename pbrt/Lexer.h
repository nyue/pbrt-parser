// ======================================================================== //
// Copyright 2015-2017 Ingo Wald                                            //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "Loc.h"

namespace pbrt_parser {

  struct PBRT_PARSER_INTERFACE Token {

    typedef enum { TOKEN_TYPE_STRING, TOKEN_TYPE_LITERAL, TOKEN_TYPE_SPECIAL } Type;

    //! constructor
    Token(const Loc &loc, 
          const Type type,
          const std::string &text);
    //! pretty-print
    std::string toString() const;
      
    inline operator std::string() const { return text// toString()
        ; }
    inline const char *c_str() const { return text.c_str(); }

    const Loc         loc;
    const std::string text;
    const Type        type;
  };

  /*! class that does the lexing - ie, the breaking up of an input
    stream of chars into an input stream of tokens.  */
  struct PBRT_PARSER_INTERFACE Lexer {

    //! constructor
    Lexer(const FileName &fn);

    std::shared_ptr<Token> next();
    std::shared_ptr<Token> peek(size_t i=0);
      
  private:
    Loc getLastLoc() { return loc; }

    inline void unget_char(int c);
    inline int get_char();
    inline bool isWhite(const char c);
    inline bool isSpecial(const char c);

    /*! produce the next token from the input stream; return NULL if
      end of (all files) is reached */
    inline std::shared_ptr<Token> produceNextToken();



    std::deque<std::shared_ptr<Token> > peekedTokens;
    std::shared_ptr<File> file;
    Loc loc;
    int peekedChar;
  };

} // ::pbrt_parser
