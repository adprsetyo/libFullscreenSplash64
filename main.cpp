#include <mod/amlmod.h>
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>

MYMOD(net.rusjj.fullsplash, GTA:SA Fullscreen Splashes, 1.0, RusJJ)

#ifdef __aarch64__
/* ============================================================
 *  64-bit (arm64-v8a) - GTA:SA 2.10 (libGTASA.so arm64)
 *  Semua patch ada di CLoadingScreen::RenderSplash (0x5203FC, 496 byte)
 *
 *  Sebelum menulis, setiap alamat diverifikasi dulu terhadap instruksi
 *  aslinya, supaya plugin tidak merusak libGTASA.so versi lain.
 * ============================================================ */
#define NOP_A64             0xD503201Fu
#define FMOV_S0_IMM(imm8)   (0x1E201000u | ((uint32_t)(imm8) << 13))   // fmov s0, #imm

struct Patch32 { uintptr_t off; uint32_t expected; uint32_t value; };

static const Patch32 g_Patches[] =
{
    // 1) Lewati pengecekan rasio layar 4:3..17:9 (3 instruksi 0x520498..0x5204C4
    //    dilompati). Setara Redirect(0x43B024 -> 0x43B05A) versi 32-bit.
    //    ldr s0,[x21,#0xc]  ->  b 0x5204C8
    { 0x520498, 0xBD400EA0u, 0x1400000Cu },

    // 2) b.hi 0x520540  ->  b.eq 0x520540   (setara BNE/BHI -> BEQ versi 32-bit)
    { 0x5204F0, 0x54000288u, 0x54000280u },

    // 3) Jalur "otherwise": menggantikan lookup tabel (adrp/add/ldr s0,[x9,x8,lsl#2])
    //    dengan  fmov s0, #0.3125  + 2x NOP        (imm8 0x54)
    { 0x5204F4, 0x900011A9u, FMOV_S0_IMM(0x54) },
    { 0x5204F8, 0x9114C129u, NOP_A64 },
    { 0x5204FC, 0xBC687920u, NOP_A64 },

    // 4) Jalur "BEQ": fmov s0, #0.5  ->  fmov s0, #0.6875               (imm8 0x66)
    { 0x520540, 0x1E2C1000u, FMOV_S0_IMM(0x66) },
};

extern "C" void OnModLoad()
{
    uintptr_t pGTASA = aml->GetLib("libGTASA.so");
    if(pGTASA == 0) return;

    // Pastikan ini memang libGTASA.so arm64 yang cocok sebelum patch apa pun.
    for(const Patch32& p : g_Patches)
    {
        if(*(volatile uint32_t*)(pGTASA + p.off) != p.expected) return;
    }
    for(const Patch32& p : g_Patches)
    {
        uint32_t v = p.value;
        aml->Write(pGTASA + p.off, (uintptr_t)&v, sizeof(v));
    }
}

#else
/* ============================================================
 *  32-bit (armeabi-v7a) - kode asli, tidak diubah
 * ============================================================ */
void Redirect(uintptr_t addr, uintptr_t to)
{
    if(!addr) return;
    if(addr & 1)
    {
        addr &= ~1;
        if (addr & 2)
        {
            aml->PlaceNOP(addr, 1);
            addr += 2;
        }
        uint32_t hook[2];
        hook[0] = 0xF000F8DF;
        hook[1] = to;
        aml->Write(addr, (uintptr_t)hook, sizeof(hook));
    }
    else
    {
        uint32_t hook[2];
        hook[0] = 0xE51FF004;
        hook[1] = to;
        aml->Write(addr, (uintptr_t)hook, sizeof(hook));
    }
}

extern "C" void OnModLoad()
{
    uintptr_t pGTASA = aml->GetLib("libGTASA.so");
    if(pGTASA == 0) return;

    aml->Write(pGTASA + 0x43B07B, (uintptr_t)"\xD0", 1); // BEQ
    aml->Write(pGTASA + 0x43B0C6, (uintptr_t)"\xB6\xEE\x06\x0A", 4); // On BEQ
    aml->Write(pGTASA + 0x43B07C, (uintptr_t)"\xB5\xEE\x04\x0A\x00\xBF\x00\xBF\x00\xBF", 10); // Otherwise

    Redirect(pGTASA + 0x43B024 + 0x1, pGTASA + 0x43B05A + 0x1); // Skip checks... Currently fullscreen splashes are for 4:3 screens and 16:9
}
#endif
