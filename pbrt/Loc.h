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

/*! \file Loc.h Defines the File and Loc structs that allow to
    pon-point each object in the scene to where it was defined in the
    file. Some scene objects (textures, plymesh etc) come with content
    that is stored in external files (png, ply, ...) whose path is
    expressed relative to the pbrt file that referenced them - so to
    find them in a later renderer we need to be able to tell where
    each such object was defined
*/

#pragma once

#include "pbrt.h"
// stl
#include <queue>
#include <memory>

namespace pbrt_parser {

  /*! file name and handle, to be used by tokenizer and loc */
  struct File {
    File(const FileName &fn);
    
    /*! get name of the file */
    std::string getFileName() const { return name; }

    /*! given the specified relative path, and this loc's path, create
        a new path that refers to the specified object, relative to
        'this' path. E.g., if this loc was stored with a texture node
        that was defined in /home/mine/model/myModel/geometry.pbrt,
        and this texture node referred to a "textures/myTex.png", then
        tex->loc.resolveRelativeFileName("textures/myTex.png") should
        return /home/mine/model/myModel/textures/myTex.png. This is
        what this helper fct does...
    */
    FileName resolveRelativeFileName(const std::string &fileRelativeToLoc) const
    {
      return name.path()+fileRelativeToLoc;
    }
      
    friend class Lexer;

  private:
    FileName name;
    FILE *file;
  };

  /*! struct referring to a 'loc'ation in the input stream, given by
    file name and line number */
  struct // PBRT_PARSER_INTERFACE
  Loc { 
    //! constructor
    Loc(std::shared_ptr<File> file);
    //! copy-constructor
    Loc(const Loc &loc);
      
    //! pretty-print
    std::string toString() const;

    FileName resolveRelativeFileName(const std::string &fileRelativeToLoc) const
    { return file->resolveRelativeFileName(fileRelativeToLoc); }
    
    friend class Lexer;
  private:
    std::shared_ptr<File> file;
    int line, col;
  };

} // :: pbrt_parser

