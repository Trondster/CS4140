/*
 * TFLite Micro inference wrapper for the dual-input drone detector.
 *
 * Architecture (INT8 PTQ):
 *   image1_input (60x80x1) ──conv─relu─conv─relu─GAP──┐
 *                                                       ├─ concat ─ dense ─ dense ─ classification (1)
 *   image2_input (60x80x1) ──conv─relu─conv─relu─GAP──┘        └─ concat ─ flatten ─ dense ─ bbox (4)
 */

#include "inference.h"
#include "drone_model_ptq_int8.h"

#include <string.h>
#include <math.h>

#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <tensorflow/lite/micro/micro_log.h>
#include <tensorflow/lite/micro/system_setup.h>
#include <tensorflow/lite/schema/schema_generated.h>

#define CONFIDENCE_THRESHOLD 0.7f

/* ── Tensor arena ────────────────────────────────────────────────────────── */
/*
 * Worst-case active tensors: both first-conv outputs (30×40×24 = 28 800 int8
 * each) alive simultaneously → ~58 KB peak.  100 KB gives headroom for TFLM
 * bookkeeping and the later fully-connected activations.
 * Reduce if the linker reports an out-of-memory error; increase (up to the
 * available SRAM) if AllocateTensors() fails at runtime.
 */
//static constexpr int kArenaSize = (163 * 1024) + 208;
static constexpr int kArenaSize = (160 * 1024) + 0;
alignas(16) static uint8_t g_arena[kArenaSize];

/* ── Static TFLM objects ─────────────────────────────────────────────────── */
static const tflite::Model    *g_model       = nullptr;
static tflite::MicroInterpreter *g_interp    = nullptr;

/* Input / output index cache (resolved by tensor name at init time) */
static int g_img1_idx  = 0;
static int g_img2_idx  = 1;
static int g_cls_idx   = 0;
static int g_bbox_idx  = 1;

/* Op resolver: one slot per distinct op type used by the model */
static tflite::MicroMutableOpResolver<10> g_resolver;

/* ── Helpers ─────────────────────────────────────────────────────────────── */
static inline int8_t quantize_u8(uint8_t val, float inv_scale, int zero_point)
{
	int q = (int)roundf((val / 255.0f) * inv_scale) + zero_point;
	if (q < -128) q = -128;
	if (q >  127) q =  127;
	return (int8_t)q;
}

/* ── Public C interface ──────────────────────────────────────────────────── */

extern "C" int drone_inference_init(void)
{
	tflite::InitializeTarget();

	g_model = tflite::GetModel(tf_drone_model_ptq);
	if (g_model->version() != TFLITE_SCHEMA_VERSION) {
		MicroPrintf("TFLM schema mismatch: model=%d, runtime=%d",
			    g_model->version(), TFLITE_SCHEMA_VERSION);
		return -1;
	}

	/* Register every op type present in the model */
	g_resolver.AddConv2D();
	g_resolver.AddRelu();
	g_resolver.AddMean();           /* GlobalAveragePooling2D */
	g_resolver.AddConcatenation();
	g_resolver.AddFullyConnected();
	g_resolver.AddReshape();        /* Flatten */
	g_resolver.AddLogistic();       /* sigmoid on classification output */
	g_resolver.AddQuantize();
	g_resolver.AddDequantize();

	static tflite::MicroInterpreter static_interp(
		g_model, g_resolver, g_arena, kArenaSize);
	g_interp = &static_interp;

	if (g_interp->AllocateTensors() != kTfLiteOk) {
		MicroPrintf("AllocateTensors() failed - arena too small?");
		return -2;
	}

	MicroPrintf("Inference ready.  Arena used: %u / %u bytes",
		    (unsigned)g_interp->arena_used_bytes(), (unsigned)kArenaSize);

	/* Resolve input indices by tensor name so ordering doesn't matter */
	for (size_t i = 0; i < g_interp->inputs_size(); i++) {
		const char *n = g_interp->input(i)->name;
		if (!n) continue;
		if (strcmp(n, "image1_input") == 0) g_img1_idx = i;
		else if (strcmp(n, "image2_input") == 0) g_img2_idx = i;
	}

	/* Resolve output indices by tensor name */
	for (size_t i = 0; i < g_interp->outputs_size(); i++) {
		const char *n = g_interp->output(i)->name;
		if (!n) continue;
		if (strcmp(n, "classification_output") == 0) g_cls_idx  = i;
		else if (strcmp(n, "bbox_output") == 0)       g_bbox_idx = i;
	}

	return 0;
}

extern "C" int drone_inference_run(const uint8_t *img1, const uint8_t *img2,
				   int n_pixels, drone_result_t *result)
{
	if (!g_interp) return -1;

	TfLiteTensor *t1 = g_interp->input(g_img1_idx);
	TfLiteTensor *t2 = g_interp->input(g_img2_idx);

	/* Quantize uint8 pixels → int8 using the model's own scale/zero_point */
	float inv_s1 = 1.0f / t1->params.scale;
	int   zp1    = t1->params.zero_point;
	float inv_s2 = 1.0f / t2->params.scale;
	int   zp2    = t2->params.zero_point;

	for (int i = 0; i < n_pixels; i++) {
		t1->data.int8[i] = quantize_u8(img1[i], inv_s1, zp1);
		t2->data.int8[i] = quantize_u8(img2[i], inv_s2, zp2);
	}

	if (g_interp->Invoke() != kTfLiteOk) {
		MicroPrintf("Invoke() failed");
		return -2;
	}

	/* Dequantize classification output (sigmoid → float confidence) */
	TfLiteTensor *cls  = g_interp->output(g_cls_idx);
	TfLiteTensor *bbox = g_interp->output(g_bbox_idx);

	result->confidence = (cls->data.int8[0] - cls->params.zero_point)
			     * cls->params.scale;
	result->detected   = result->confidence > CONFIDENCE_THRESHOLD;

	/* Dequantize bbox: [x, y, w, h] normalized to [0, 1] */
	float bs = bbox->params.scale;
	int   bz = bbox->params.zero_point;
	result->x = (bbox->data.int8[0] - bz) * bs;
	result->y = (bbox->data.int8[1] - bz) * bs;
	result->w = (bbox->data.int8[2] - bz) * bs;
	result->h = (bbox->data.int8[3] - bz) * bs;

	return 0;
}
