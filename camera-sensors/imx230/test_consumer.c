/* SPDX-License-Identifier: GPL-2.0-only */
/* Offline ARM32 fixture loader. Runs only selected, pinned pure consumer paths.
 * No ELF constructors, camera services, device access or system dynamic linker.
 * Every unsupported imported function terminates the test immediately.
 */
#include "angler_sensor_abi.h"

struct elf_header {
    uint8_t ident[16]; uint16_t type, machine;
    uint32_t version, entry, phoff, shoff, flags;
    uint16_t ehsize, phsize, phnum, shsize, shnum, strings;
};
struct elf_segment { uint32_t type, offset, va, pa, filesz, memsz, flags, alignment; };
struct elf_section { uint32_t name, type, flags, va, offset, size, link, info, align, stride; };
struct elf_symbol { uint32_t name, value, size; uint8_t info, other; uint16_t section; };
struct elf_rel { uint32_t offset, info; };
struct elf_dynamic { uint32_t tag, value; };

extern const uint8_t _binary_consumer_so_start[], _binary_reference_so_start[], _binary_candidate_so_start[];
static uint8_t consumer_memory[0x40000] __attribute__((aligned(4096)));
static uint8_t reference_memory[0x10000] __attribute__((aligned(4096)));
static uint8_t candidate_memory[0x20000] __attribute__((aligned(4096)));
static uint32_t zero_data[1024];
static uint32_t guard = 0x349fc7ab;
static uint32_t output[0x6a8 / 4 + 2];

static __attribute__((noreturn)) void finish(uint32_t status)
{
    register uint32_t r0 __asm__("r0") = status;
    register uint32_t r7 __asm__("r7") = 1;
    __asm__ volatile("svc #0" : : "r"(r0), "r"(r7) : "memory");
    __builtin_unreachable();
}
static void unexpected_import(void) { finish(90); }
static int log_stub(void) { return 0; }
static void *copy_bytes(void *destination, const void *source, uint32_t n)
{
    uint8_t *d = destination; const uint8_t *s = source;
    while (n--) *d++ = *s++;
    return destination;
}
static void *clear_bytes(void *destination, int value, uint32_t n)
{
    uint8_t *d = destination;
    while (n--) *d++ = (uint8_t)value;
    return destination;
}
void *memset(void *destination, int value, __SIZE_TYPE__ n)
{
    return clear_bytes(destination, value, n);
}
static int same(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
static uint32_t copy_string(char *destination, const char *source, uint32_t capacity)
{
    uint32_t n = 0;
    while (source[n]) { if (n + 1 < capacity) destination[n] = source[n]; n++; }
    if (capacity) destination[n < capacity ? n : capacity - 1] = 0;
    return n;
}
static char *copy_string_padded(char *destination, const char *source, uint32_t capacity)
{
    uint32_t i = 0;
    while (i < capacity && source[i]) { destination[i] = source[i]; i++; }
    while (i < capacity) destination[i++] = 0;
    return destination;
}
static uint32_t imported(const char *name, unsigned int type)
{
    if (same(name, "memcpy") || same(name, "__aeabi_memcpy")) return (uint32_t)copy_bytes;
    if (same(name, "memset")) return (uint32_t)clear_bytes;
    if (same(name, "strlcpy")) return (uint32_t)copy_string;
    if (same(name, "strncpy")) return (uint32_t)copy_string_padded;
    if (same(name, "__android_log_print")) return (uint32_t)log_stub;
    if (same(name, "__stack_chk_guard")) return (uint32_t)&guard;
    if (type == 1) return (uint32_t)zero_data;
    return (uint32_t)unexpected_import;
}
static void relocate(uint8_t *memory, uint32_t rel, uint32_t size,
                     struct elf_symbol *symbols, const char *strings)
{
    uint32_t i;
    struct elf_rel *entries = (void *)(memory + rel);
    for (i = 0; i < size / sizeof(*entries); i++) {
        uint32_t *place = (void *)(memory + entries[i].offset);
        uint32_t type = entries[i].info & 255;
        struct elf_symbol *s = &symbols[entries[i].info >> 8];
        uint32_t value;
        if (type == 0) continue;
        if (type == 23) { *place += (uint32_t)memory; continue; }
        value = s->section ? (uint32_t)memory + s->value : imported(strings + s->name, s->info & 15);
        if (type == 21 || type == 22) *place = value;
        else if (type == 2) *place += value;
        else finish(91);
    }
}
static void load(const uint8_t *file, uint8_t *memory, uint32_t capacity)
{
    const struct elf_header *h = (const void *)file;
    struct elf_dynamic *dynamic = 0;
    struct elf_symbol *symbols = 0;
    const char *strings = 0;
    uint32_t rel = 0, rel_size = 0, plt = 0, plt_size = 0, i;
    register uint32_t r0 __asm__("r0") = (uint32_t)memory;
    register uint32_t r1 __asm__("r1") = capacity;
    register uint32_t r2 __asm__("r2") = 7;
    register uint32_t r7 __asm__("r7") = 125;
    if (h->machine != 40 || h->ident[4] != 1 || h->ident[5] != 1) finish(92);
    __asm__ volatile("svc #0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory");
    if (r0) finish(93);
    for (i = 0; i < h->phnum; i++) {
        const struct elf_segment *p = (const void *)(file + h->phoff + i * h->phsize);
        if (p->type == 1) {
            if (p->va > capacity || p->memsz > capacity - p->va || p->filesz > p->memsz) finish(94);
            copy_bytes(memory + p->va, file + p->offset, p->filesz);
        }
        if (p->type == 2) dynamic = (void *)(memory + p->va);
    }
    if (!dynamic) finish(95);
    for (; dynamic->tag; dynamic++) {
        switch (dynamic->tag) {
        case 5: strings = (void *)(memory + dynamic->value); break;
        case 6: symbols = (void *)(memory + dynamic->value); break;
        case 17: rel = dynamic->value; break;
        case 18: rel_size = dynamic->value; break;
        case 23: plt = dynamic->value; break;
        case 2: plt_size = dynamic->value; break;
        }
    }
    if (!symbols || !strings) finish(96);
    relocate(memory, rel, rel_size, symbols, strings);
    relocate(memory, plt, plt_size, symbols, strings);
}
static uint32_t symbol(const uint8_t *file, const char *name)
{
    const struct elf_header *h = (const void *)file;
    unsigned int i, j;
    for (i = 0; i < h->shnum; i++) {
        const struct elf_section *s = (const void *)(file + h->shoff + i * h->shsize);
        const struct elf_section *str;
        if (s->type != 11) continue;
        str = (const void *)(file + h->shoff + s->link * h->shsize);
        for (j = 0; j < s->size / sizeof(struct elf_symbol); j++) {
            const struct elf_symbol *entry = (const void *)(file + s->offset + j * sizeof(*entry));
            if (same((const char *)file + str->offset + entry->name, name)) return entry->value;
        }
    }
    finish(97);
}

int run_tests(void)
{
    uint32_t context[32] = {0};
    int32_t (*caps)(void *, void *);
    uint32_t (*fourcc)(uint32_t, uint32_t, uint32_t);
    void *(*reference_open)(void);
    void *(*candidate_open)(void);
    struct angler_sensor_descriptor *descriptor;
    void (*register_setting)(void *, const void *);
    void (*slave_info)(void *, const void *, void *, void *);
    void (*csid_info)(void *, const void *, void *);
    struct msm_camera_sensor_slave_info translated_slave = {.slave_addr = 0};
    struct msm_sensor_power_setting up[12] = {{0}}, down[12] = {{0}};
    struct msm_camera_i2c_reg_setting translated_setting;
    struct msm_camera_csid_params translated_csid = {0};
    struct msm_camera_csid_vc_cfg cid[16] = {{0}};
    unsigned int i;
    load(_binary_consumer_so_start, consumer_memory, sizeof(consumer_memory));
    load(_binary_reference_so_start, reference_memory, sizeof(reference_memory));
    load(_binary_candidate_so_start, candidate_memory, sizeof(candidate_memory));
    caps = (void *)(consumer_memory + 0x20c89);
    fourcc = (void *)(consumer_memory + symbol(_binary_consumer_so_start, "sensor_util_get_fourcc_format"));
    reference_open = (void *)(reference_memory + symbol(_binary_reference_so_start, "sensor_open_lib"));
    candidate_open = (void *)(candidate_memory + symbol(_binary_candidate_so_start, "sensor_open_lib"));
    descriptor = candidate_open();
    context[1] = (uint32_t)reference_open();
    if (caps(context, output) || ((uint8_t *)output)[0x30] != 3 || output[0x34 / 4] != 4080) return 101;
    clear_bytes(output, 0, sizeof(output));
    context[1] = (uint32_t)descriptor;
    output[0x6a8 / 4] = 0x5a5a5a5a;
    if (caps(context, output)) return 102;
    if (((uint8_t *)output)[0x30] != 1 || output[0x34 / 4] != 2496 || output[0x38 / 4] != 1872) return 103;
    if (output[0x6a8 / 4] != 0x5a5a5a5a) return 104;
    if (!output[0x19c / 4] || output[0x3e4 / 4] != 5344 || output[0x3e8 / 4] != 4016) return 105;
    /* BA10: kernel's SGRBG10 fourcc, from the actual angler conversion helper. */
    if (fourcc(0, descriptor->output.bayer_order, CSI_DECODE_10BIT) != 0x30314142u) return 106;
    register_setting = (void *)(consumer_memory + symbol(_binary_consumer_so_start, "translate_sensor_reg_setting"));
    for (i = 0; i < descriptor->init.count; i++) {
        struct msm_camera_i2c_reg_setting *source = &descriptor->init.entries[i];
        register_setting(&translated_setting, source);
        if (translated_setting.reg_setting != source->reg_setting || translated_setting.size != source->size ||
            translated_setting.addr_type != MSM_CAMERA_I2C_WORD_ADDR || translated_setting.data_type != source->data_type) return 107;
    }
    slave_info = (void *)(consumer_memory + symbol(_binary_consumer_so_start, "translate_sensor_slave_info"));
    slave_info(&translated_slave, &descriptor->slave, up, down);
    if (translated_slave.slave_addr != 0x20 || translated_slave.sensor_id_info.sensor_id != 0x0230 ||
        translated_slave.power_setting_array.size != 7 || translated_slave.power_setting_array.size_down != 5) return 108;
    for (i = 0; i < translated_slave.power_setting_array.size; i++) {
        struct msm_sensor_power_setting *source = &descriptor->slave.power_setting_array.power_setting_a[i];
        if (up[i].seq_type != source->seq_type || up[i].seq_val != source->seq_val ||
            up[i].config_val != source->config_val || up[i].delay != source->delay) return 109;
    }
    csid_info = (void *)(consumer_memory + symbol(_binary_consumer_so_start, "translate_sensor_csid_params"));
    csid_info(&translated_csid, &descriptor->csi.entries[0].csid, cid);
    if (translated_csid.lane_assign != 0x0423 || translated_csid.lane_cnt != 4 ||
        translated_csid.lut_params.num_cid != 1 || translated_csid.lut_params.vc_cfg[0] != cid ||
        cid[0].dt != CSI_RAW10 || cid[0].decode_format != CSI_DECODE_10BIT) return 110;
    /* Negative control: the real consumer must reject a descriptor with no modes. */
    descriptor->modes.count = 0;
    if (!caps(context, output)) return 111;
    descriptor->modes.count = 1;
    return 0;
}
