//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

//#include "font/w_font.hpp"
/*
namespace Wiesel {
  Font::Font(const std::string& fileName) {
    FT_Library  library;
    FT_Face     face;


    error = FT_Init_FreeType( &library );
    if ( error ) { ... }

    error = FT_New_Face( library,
                        "/usr/share/fonts/truetype/arial.ttf",
                        0,
                        &face );
    if ( error == FT_Err_Unknown_File_Format )
    {
      ... the font file could be opened and read, but it appears
          ... that its font format is unsupported
    }
    else if ( error )
    {
      ... another error code means that the font file could not
          ... be opened or read, or that it is broken...
    }
  }
}*/