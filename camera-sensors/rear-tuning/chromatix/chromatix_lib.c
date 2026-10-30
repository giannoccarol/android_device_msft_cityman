/* The talkman chromatix container.
 *
 * Compiled once per tuning name, with -DCHX_TYPE and -DCHX_DATA selecting the
 * struct class and the payload.  See chromatix_talkman.h for what a chromatix
 * library is and how the layout was established.
 *
 * The donor's own container is three instructions and one relocation; this
 * reproduces both exactly:
 *
 *   .data + 0   a self-pointer  (the file's single R_ARM_RELATIVE)
 *   .data + 4   the tuning struct
 *   load_chromatix()  returns .data + 4
 *
 * `self` is not read by anything we can find -- the daemon dlopens the file and
 * calls load_chromatix() -- but it costs one word and keeps the layout a
 * one-variable match with the library the daemon has always loaded.
 */
#include "chromatix_structs.h"

static struct chx_container {
	void     *self;
	CHX_TYPE  chx;
} chx_blob = {
	.self = &chx_blob,
	.chx  = {
#include CHX_DATA
	},
};

CHX_STATIC_ASSERT(__builtin_offsetof(struct chx_container, chx) == 4,
                  container_struct_at_4);

void *load_chromatix(void);
void *load_chromatix(void)
{
	return &chx_blob.chx;
}
