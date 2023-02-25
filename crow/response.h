#ifndef _CROW_FRACTAL_SERVER_RESPONSE_
#define _CROW_FRACTAL_SERVER_RESPONSE_

#include <utility>
#include <string>

#include <crow.h>

crow::response ImageWithMetadata(std::string png_contents,
				 const crow::json::wvalue& metadata) {

  crow::multipart::part png_part;
  png_part.body = std::move(png_contents);
  {
    crow::multipart::header content_type_header;
    content_type_header.value = "image/png";
    crow::multipart::header content_disposition_header;
    content_disposition_header.value = "form-data";
    content_disposition_header.params["name"] = "fractal_image";
    content_disposition_header.params["filename"] = "fractal_image.png";
    png_part.headers.emplace("Content-Type", content_type_header);
    png_part.headers.emplace("Content-Disposition", content_disposition_header);
  }

  crow::multipart::part metadata_part;
  metadata_part.body = metadata.dump();
  {
    crow::multipart::header content_type_header;
    content_type_header.value = "application/json";
    crow::multipart::header content_disposition_header;
    content_disposition_header.value = "form-data";
    content_disposition_header.params["name"] = "metadata";
    metadata_part.headers.emplace("Content-Type", content_type_header);
    metadata_part.headers.emplace("Content-Disposition", content_disposition_header);
  }

  return crow::response(crow::multipart::message(/*header=*/{},
						 /*boudary=*/"CROW-BOUNDARY",
						 /*parts=*/{png_part, metadata_part}));
}

#endif // _CROW_FRACTAL_SERVER_RESPONSE_
