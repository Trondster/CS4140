#ifndef INFERENCE_H_
#define INFERENCE_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct {
	float confidence;
	float x, y, w, h;  /* normalized bbox [0, 1] */
	bool  detected;
} drone_result_t;

#ifdef __cplusplus
extern "C" {
#endif

int drone_inference_init(void);

/*
 * img1: current grayscale frame  (SCALED_W * SCALED_H bytes)
 * img2: motion-diff frame        (SCALED_W * SCALED_H bytes)
 * n_pixels: SCALED_W * SCALED_H
 */
int drone_inference_run(const uint8_t *img1, const uint8_t *img2,
			int n_pixels, drone_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* INFERENCE_H_ */
