/* Trouble code texts, see dtc_text.h.
 *
 * Generic SAE J2012 / ISO 15031-6 meanings, kept short for a small screen,
 * upper case (the round gauge's fonts carry the Czech capitals only). The
 * list holds what a common-rail diesel like the T5.1's CAAC tends to set;
 * anything else falls back to its group. Manufacturer codes (P1xxx, P3xxx
 * and the like) mean whatever the maker says, so they only get the group.
 */
#include "dtc_text.h"

#include <stddef.h>

typedef struct {
    uint16_t code;
    const char *en, *cs;
} dtc_text_t;

/* P0xxx = 0x0xxx, P2xxx = 0x2xxx; sorted */
static const dtc_text_t T[] = {
    { 0x0045, "BOOST CONTROL SOLENOID CIRCUIT", "OBVOD VENTILU ŘÍZENÍ PLNĚNÍ" },
    { 0x0046, "BOOST CONTROL SOLENOID RANGE", "VENTIL ŘÍZENÍ PLNĚNÍ ROZSAH" },
    { 0x0047, "BOOST CONTROL SOLENOID LOW", "VENTIL ŘÍZENÍ PLNĚNÍ NÍZKÉ" },
    { 0x0048, "BOOST CONTROL SOLENOID HIGH", "VENTIL ŘÍZENÍ PLNĚNÍ VYSOKÉ" },
    { 0x0087, "FUEL RAIL PRESSURE TOO LOW", "TLAK V RAILU PŘÍLIŠ NÍZKÝ" },
    { 0x0088, "FUEL RAIL PRESSURE TOO HIGH", "TLAK V RAILU PŘÍLIŠ VYSOKÝ" },
    { 0x0100, "AIR MASS METER CIRCUIT", "OBVOD MĚŘIČE HMOTNOSTI VZDUCHU" },
    { 0x0101, "AIR MASS METER RANGE", "MĚŘIČ HMOTNOSTI VZDUCHU ROZSAH" },
    { 0x0102, "AIR MASS METER LOW", "MĚŘIČ HMOTNOSTI VZDUCHU NÍZKÝ" },
    { 0x0103, "AIR MASS METER HIGH", "MĚŘIČ HMOTNOSTI VZDUCHU VYSOKÝ" },
    { 0x0104, "AIR MASS METER INTERMITTENT", "MĚŘIČ HMOTNOSTI VZDUCHU PŘERUŠOVANĚ" },
    { 0x0105, "BOOST PRESSURE SENSOR CIRCUIT", "OBVOD SNÍMAČE TLAKU SÁNÍ" },
    { 0x0106, "BOOST PRESSURE SENSOR RANGE", "SNÍMAČ TLAKU SÁNÍ ROZSAH" },
    { 0x0107, "BOOST PRESSURE SENSOR LOW", "SNÍMAČ TLAKU SÁNÍ NÍZKÝ" },
    { 0x0108, "BOOST PRESSURE SENSOR HIGH", "SNÍMAČ TLAKU SÁNÍ VYSOKÝ" },
    { 0x0109, "BOOST PRESSURE SENSOR INTERMITTENT", "SNÍMAČ TLAKU SÁNÍ PŘERUŠOVANĚ" },
    { 0x0110, "INTAKE AIR TEMP SENSOR CIRCUIT", "OBVOD SNÍMAČE TEPLOTY NASÁVÁNÍ" },
    { 0x0112, "INTAKE AIR TEMP SENSOR LOW", "SNÍMAČ TEPLOTY NASÁVÁNÍ NÍZKÝ" },
    { 0x0113, "INTAKE AIR TEMP SENSOR HIGH", "SNÍMAČ TEPLOTY NASÁVÁNÍ VYSOKÝ" },
    { 0x0114, "INTAKE AIR TEMP SENSOR INTERMITTENT", "SNÍMAČ TEPLOTY NASÁVÁNÍ PŘERUŠOVANĚ" },
    { 0x0115, "COOLANT TEMP SENSOR CIRCUIT", "OBVOD SNÍMAČE TEPLOTY VODY" },
    { 0x0116, "COOLANT TEMP SENSOR RANGE", "SNÍMAČ TEPLOTY VODY ROZSAH" },
    { 0x0117, "COOLANT TEMP SENSOR LOW", "SNÍMAČ TEPLOTY VODY NÍZKÝ" },
    { 0x0118, "COOLANT TEMP SENSOR HIGH", "SNÍMAČ TEPLOTY VODY VYSOKÝ" },
    { 0x0119, "COOLANT TEMP SENSOR INTERMITTENT", "SNÍMAČ TEPLOTY VODY PŘERUŠOVANĚ" },
    { 0x0190, "RAIL PRESSURE SENSOR CIRCUIT", "OBVOD SNÍMAČE TLAKU V RAILU" },
    { 0x0191, "RAIL PRESSURE SENSOR RANGE", "SNÍMAČ TLAKU V RAILU ROZSAH" },
    { 0x0192, "RAIL PRESSURE SENSOR LOW", "SNÍMAČ TLAKU V RAILU NÍZKÝ" },
    { 0x0193, "RAIL PRESSURE SENSOR HIGH", "SNÍMAČ TLAKU V RAILU VYSOKÝ" },
    { 0x0201, "INJECTOR CIRCUIT CYLINDER 1", "OBVOD VSTŘIKOVAČE VÁLEC 1" },
    { 0x0202, "INJECTOR CIRCUIT CYLINDER 2", "OBVOD VSTŘIKOVAČE VÁLEC 2" },
    { 0x0203, "INJECTOR CIRCUIT CYLINDER 3", "OBVOD VSTŘIKOVAČE VÁLEC 3" },
    { 0x0204, "INJECTOR CIRCUIT CYLINDER 4", "OBVOD VSTŘIKOVAČE VÁLEC 4" },
    { 0x0234, "TURBO OVERBOOST", "PŘEPLŇOVÁNÍ PŘÍLIŠ VYSOKÉ" },
    { 0x0299, "TURBO UNDERBOOST", "PŘEPLŇOVÁNÍ PŘÍLIŠ NÍZKÉ" },
    { 0x0300, "RANDOM / MULTIPLE MISFIRE", "VYNECHÁVÁNÍ VÍCE VÁLCŮ" },
    { 0x0301, "MISFIRE CYLINDER 1", "VYNECHÁVÁNÍ VÁLEC 1" },
    { 0x0302, "MISFIRE CYLINDER 2", "VYNECHÁVÁNÍ VÁLEC 2" },
    { 0x0303, "MISFIRE CYLINDER 3", "VYNECHÁVÁNÍ VÁLEC 3" },
    { 0x0304, "MISFIRE CYLINDER 4", "VYNECHÁVÁNÍ VÁLEC 4" },
    { 0x0335, "CRANKSHAFT SENSOR CIRCUIT", "OBVOD SNÍMAČE KLIKOVÉ HŘÍDELE" },
    { 0x0340, "CAMSHAFT SENSOR CIRCUIT", "OBVOD SNÍMAČE VAČKOVÉ HŘÍDELE" },
    { 0x0380, "GLOW PLUG CIRCUIT", "OBVOD ŽHAVENÍ" },
    { 0x0400, "EGR FLOW", "PRŮTOK EGR" },
    { 0x0401, "EGR FLOW INSUFFICIENT", "PRŮTOK EGR NEDOSTATEČNÝ" },
    { 0x0402, "EGR FLOW EXCESSIVE", "PRŮTOK EGR NADMĚRNÝ" },
    { 0x0403, "EGR CONTROL CIRCUIT", "OBVOD OVLÁDÁNÍ EGR" },
    { 0x0420, "CATALYST EFFICIENCY LOW", "ÚČINNOST KATALYZÁTORU NÍZKÁ" },
    { 0x0470, "EXHAUST PRESSURE SENSOR", "SNÍMAČ TLAKU VÝFUKU" },
    { 0x0480, "COOLING FAN 1 CONTROL CIRCUIT", "OBVOD OVLÁDÁNÍ VENTILÁTORU 1" },
    { 0x0500, "VEHICLE SPEED SENSOR", "SNÍMAČ RYCHLOSTI" },
    { 0x0544, "EXHAUST TEMP SENSOR CIRCUIT", "OBVOD SNÍMAČE TEPLOTY VÝFUKU" },
    { 0x0545, "EXHAUST TEMP SENSOR LOW", "SNÍMAČ TEPLOTY VÝFUKU NÍZKÝ" },
    { 0x0546, "EXHAUST TEMP SENSOR HIGH", "SNÍMAČ TEPLOTY VÝFUKU VYSOKÝ" },
    { 0x0560, "SYSTEM VOLTAGE", "NAPĚTÍ PALUBNÍ SÍTĚ" },
    { 0x0562, "SYSTEM VOLTAGE LOW", "NAPĚTÍ PALUBNÍ SÍTĚ NÍZKÉ" },
    { 0x0563, "SYSTEM VOLTAGE HIGH", "NAPĚTÍ PALUBNÍ SÍTĚ VYSOKÉ" },
    { 0x0606, "CONTROL MODULE PROCESSOR", "PROCESOR ŘÍDICÍ JEDNOTKY" },
    { 0x0670, "GLOW PLUG MODULE CIRCUIT", "OBVOD JEDNOTKY ŽHAVENÍ" },
    { 0x0671, "GLOW PLUG CYLINDER 1", "ŽHAVICÍ SVÍČKA VÁLEC 1" },
    { 0x0672, "GLOW PLUG CYLINDER 2", "ŽHAVICÍ SVÍČKA VÁLEC 2" },
    { 0x0673, "GLOW PLUG CYLINDER 3", "ŽHAVICÍ SVÍČKA VÁLEC 3" },
    { 0x0674, "GLOW PLUG CYLINDER 4", "ŽHAVICÍ SVÍČKA VÁLEC 4" },
    { 0x0700, "GEARBOX FAULT, READ ITS ECU", "ZÁVADA PŘEVODOVKY" },
    { 0x2002, "DPF EFFICIENCY LOW", "ÚČINNOST DPF NÍZKÁ" },
    { 0x2263, "BOOST SYSTEM PERFORMANCE", "SYSTÉM PŘEPLŇOVÁNÍ VÝKON" },
    { 0x242F, "DPF RESTRICTED, ASH", "DPF ZANESENÝ POPELEM" },
    { 0x2452, "DPF PRESSURE SENSOR CIRCUIT", "OBVOD SNÍMAČE TLAKU DPF" },
    { 0x2453, "DPF PRESSURE SENSOR RANGE", "SNÍMAČ TLAKU DPF ROZSAH" },
    { 0x2454, "DPF PRESSURE SENSOR LOW", "SNÍMAČ TLAKU DPF NÍZKÝ" },
    { 0x2455, "DPF PRESSURE SENSOR HIGH", "SNÍMAČ TLAKU DPF VYSOKÝ" },
    { 0x2463, "DPF RESTRICTED, SOOT", "DPF ZANESENÝ SAZEMI" },
};

const char *dtc_text(uint16_t code, int cs)
{
    int lo = 0, hi = (int)(sizeof T / sizeof T[0]) - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (T[m].code == code) return cs ? T[m].cs : T[m].en;
        if (T[m].code < code) lo = m + 1;
        else                  hi = m - 1;
    }
    return NULL;
}

const char *dtc_group(uint16_t code, int cs)
{
    int letter = code >> 14, d1 = (code >> 12) & 3, d2 = (code >> 8) & 15;
    if (letter == 1) return cs ? "PODVOZEK" : "CHASSIS";
    if (letter == 2) return cs ? "KAROSERIE" : "BODY";
    if (letter == 3) return cs ? "KOMUNIKACE" : "NETWORK";
    /* P: 0 and 2 generic, 1 and 3 the maker's */
    if (d1 == 1 || d1 == 3) {
        return cs ? "KÓD VÝROBCE, VIZ VCDS" : "MAKER'S CODE, SEE VCDS";
    }
    switch (d2) {
    case 0: case 1: case 2: return cs ? "PALIVO A VZDUCH" : "FUEL AND AIR";
    case 3:                 return cs ? "ZAPALOVÁNÍ / VYNECHÁVÁNÍ" : "IGNITION / MISFIRE";
    case 4:                 return cs ? "EMISE" : "EMISSIONS";
    case 5:                 return cs ? "RYCHLOST, VOLNOBĚH" : "SPEED, IDLE";
    case 6:                 return cs ? "ŘÍDICÍ JEDNOTKA" : "ECU, OUTPUTS";
    case 7: case 8: case 9: return cs ? "PŘEVODOVKA" : "GEARBOX";
    default:                return cs ? "MOTOR" : "ENGINE";
    }
}

const char *dtc_nrc_text(uint8_t nrc, int cs)
{
    switch (nrc) {
    case 0x22: return cs ? "MOTOR VYP, KLÍČ ZAP" : "ENGINE OFF, KEY ON";
    case 0x11: return cs ? "JEDNOTKA TO NEUMÍ" : "NOT SUPPORTED";
    case 0x33: return cs ? "JEDNOTKA ZAMČENÁ" : "SECURITY ACCESS DENIED";
    default:   return cs ? "JEDNOTKA ODMÍTLA" : "REFUSED BY THE ECU";
    }
}
