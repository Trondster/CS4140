#ifndef PREPROC_DIRECT_H_
#define PREPROC_DIRECT_H_

#include <cstdint>
#include "i_preproc_handler.hpp"

//Implements IPreprocHandler
class PreprocDirect : public IPreprocHandler {
public:
    PreprocDirect(uint8_t* input_buf, uint8_t* second_buf, uint8_t* grayscale_buf, uint8_t* second_grayscale_buf,
                  int height, int width, int bpp) :
        IPreprocHandler(input_buf, second_buf, grayscale_buf, second_grayscale_buf, height, width, bpp) {}

    const char* get_name() override {
        return "Direct passthrough";
    }
};

#endif /* PREPROC_DIRECT_H_ */