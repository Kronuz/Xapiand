/*
 * Copyright (c) 2019 German Mendez Bravo (Kronuz)
 * Copyright (c) 2012-2019 Anton Bachin
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ENUM_H
#define ENUM_H


#define _ENUM_M1(c, m, d, x) m(d, c-1, x)
#define _ENUM_M2(c, m, d, x, ...) m(d, c-2, x) _ENUM_M1(c, m, d, __VA_ARGS__)
#define _ENUM_M3(c, m, d, x, ...) m(d, c-3, x) _ENUM_M2(c, m, d, __VA_ARGS__)
#define _ENUM_M4(c, m, d, x, ...) m(d, c-4, x) _ENUM_M3(c, m, d, __VA_ARGS__)
#define _ENUM_M5(c, m, d, x, ...) m(d, c-5, x) _ENUM_M4(c, m, d, __VA_ARGS__)
#define _ENUM_M6(c, m, d, x, ...) m(d, c-6, x) _ENUM_M5(c, m, d, __VA_ARGS__)
#define _ENUM_M7(c, m, d, x, ...) m(d, c-7, x) _ENUM_M6(c, m, d, __VA_ARGS__)
#define _ENUM_M8(c, m, d, x, ...) m(d, c-8, x) _ENUM_M7(c, m, d, __VA_ARGS__)
#define _ENUM_M9(c, m, d, x, ...) m(d, c-9, x) _ENUM_M8(c, m, d, __VA_ARGS__)
#define _ENUM_M10(c, m, d, x, ...) m(d, c-10, x) _ENUM_M9(c, m, d, __VA_ARGS__)
#define _ENUM_M11(c, m, d, x, ...) m(d, c-11, x) _ENUM_M10(c, m, d, __VA_ARGS__)
#define _ENUM_M12(c, m, d, x, ...) m(d, c-12, x) _ENUM_M11(c, m, d, __VA_ARGS__)
#define _ENUM_M13(c, m, d, x, ...) m(d, c-13, x) _ENUM_M12(c, m, d, __VA_ARGS__)
#define _ENUM_M14(c, m, d, x, ...) m(d, c-14, x) _ENUM_M13(c, m, d, __VA_ARGS__)
#define _ENUM_M15(c, m, d, x, ...) m(d, c-15, x) _ENUM_M14(c, m, d, __VA_ARGS__)
#define _ENUM_M16(c, m, d, x, ...) m(d, c-16, x) _ENUM_M15(c, m, d, __VA_ARGS__)
#define _ENUM_M17(c, m, d, x, ...) m(d, c-17, x) _ENUM_M16(c, m, d, __VA_ARGS__)
#define _ENUM_M18(c, m, d, x, ...) m(d, c-18, x) _ENUM_M17(c, m, d, __VA_ARGS__)
#define _ENUM_M19(c, m, d, x, ...) m(d, c-19, x) _ENUM_M18(c, m, d, __VA_ARGS__)
#define _ENUM_M20(c, m, d, x, ...) m(d, c-20, x) _ENUM_M19(c, m, d, __VA_ARGS__)
#define _ENUM_M21(c, m, d, x, ...) m(d, c-21, x) _ENUM_M20(c, m, d, __VA_ARGS__)
#define _ENUM_M22(c, m, d, x, ...) m(d, c-22, x) _ENUM_M21(c, m, d, __VA_ARGS__)
#define _ENUM_M23(c, m, d, x, ...) m(d, c-23, x) _ENUM_M22(c, m, d, __VA_ARGS__)
#define _ENUM_M24(c, m, d, x, ...) m(d, c-24, x) _ENUM_M23(c, m, d, __VA_ARGS__)
#define _ENUM_M25(c, m, d, x, ...) m(d, c-25, x) _ENUM_M24(c, m, d, __VA_ARGS__)
#define _ENUM_M26(c, m, d, x, ...) m(d, c-26, x) _ENUM_M25(c, m, d, __VA_ARGS__)
#define _ENUM_M27(c, m, d, x, ...) m(d, c-27, x) _ENUM_M26(c, m, d, __VA_ARGS__)
#define _ENUM_M28(c, m, d, x, ...) m(d, c-28, x) _ENUM_M27(c, m, d, __VA_ARGS__)
#define _ENUM_M29(c, m, d, x, ...) m(d, c-29, x) _ENUM_M28(c, m, d, __VA_ARGS__)
#define _ENUM_M30(c, m, d, x, ...) m(d, c-30, x) _ENUM_M29(c, m, d, __VA_ARGS__)
#define _ENUM_M31(c, m, d, x, ...) m(d, c-31, x) _ENUM_M30(c, m, d, __VA_ARGS__)
#define _ENUM_M32(c, m, d, x, ...) m(d, c-32, x) _ENUM_M31(c, m, d, __VA_ARGS__)
#define _ENUM_M33(c, m, d, x, ...) m(d, c-33, x) _ENUM_M32(c, m, d, __VA_ARGS__)
#define _ENUM_M34(c, m, d, x, ...) m(d, c-34, x) _ENUM_M33(c, m, d, __VA_ARGS__)
#define _ENUM_M35(c, m, d, x, ...) m(d, c-35, x) _ENUM_M34(c, m, d, __VA_ARGS__)
#define _ENUM_M36(c, m, d, x, ...) m(d, c-36, x) _ENUM_M35(c, m, d, __VA_ARGS__)
#define _ENUM_M37(c, m, d, x, ...) m(d, c-37, x) _ENUM_M36(c, m, d, __VA_ARGS__)
#define _ENUM_M38(c, m, d, x, ...) m(d, c-38, x) _ENUM_M37(c, m, d, __VA_ARGS__)
#define _ENUM_M39(c, m, d, x, ...) m(d, c-39, x) _ENUM_M38(c, m, d, __VA_ARGS__)
#define _ENUM_M40(c, m, d, x, ...) m(d, c-40, x) _ENUM_M39(c, m, d, __VA_ARGS__)
#define _ENUM_M41(c, m, d, x, ...) m(d, c-41, x) _ENUM_M40(c, m, d, __VA_ARGS__)
#define _ENUM_M42(c, m, d, x, ...) m(d, c-42, x) _ENUM_M41(c, m, d, __VA_ARGS__)
#define _ENUM_M43(c, m, d, x, ...) m(d, c-43, x) _ENUM_M42(c, m, d, __VA_ARGS__)
#define _ENUM_M44(c, m, d, x, ...) m(d, c-44, x) _ENUM_M43(c, m, d, __VA_ARGS__)
#define _ENUM_M45(c, m, d, x, ...) m(d, c-45, x) _ENUM_M44(c, m, d, __VA_ARGS__)
#define _ENUM_M46(c, m, d, x, ...) m(d, c-46, x) _ENUM_M45(c, m, d, __VA_ARGS__)
#define _ENUM_M47(c, m, d, x, ...) m(d, c-47, x) _ENUM_M46(c, m, d, __VA_ARGS__)
#define _ENUM_M48(c, m, d, x, ...) m(d, c-48, x) _ENUM_M47(c, m, d, __VA_ARGS__)
#define _ENUM_M49(c, m, d, x, ...) m(d, c-49, x) _ENUM_M48(c, m, d, __VA_ARGS__)
#define _ENUM_M50(c, m, d, x, ...) m(d, c-50, x) _ENUM_M49(c, m, d, __VA_ARGS__)
#define _ENUM_M51(c, m, d, x, ...) m(d, c-51, x) _ENUM_M50(c, m, d, __VA_ARGS__)
#define _ENUM_M52(c, m, d, x, ...) m(d, c-52, x) _ENUM_M51(c, m, d, __VA_ARGS__)
#define _ENUM_M53(c, m, d, x, ...) m(d, c-53, x) _ENUM_M52(c, m, d, __VA_ARGS__)
#define _ENUM_M54(c, m, d, x, ...) m(d, c-54, x) _ENUM_M53(c, m, d, __VA_ARGS__)
#define _ENUM_M55(c, m, d, x, ...) m(d, c-55, x) _ENUM_M54(c, m, d, __VA_ARGS__)
#define _ENUM_M56(c, m, d, x, ...) m(d, c-56, x) _ENUM_M55(c, m, d, __VA_ARGS__)
#define _ENUM_M57(c, m, d, x, ...) m(d, c-57, x) _ENUM_M56(c, m, d, __VA_ARGS__)
#define _ENUM_M58(c, m, d, x, ...) m(d, c-58, x) _ENUM_M57(c, m, d, __VA_ARGS__)
#define _ENUM_M59(c, m, d, x, ...) m(d, c-59, x) _ENUM_M58(c, m, d, __VA_ARGS__)
#define _ENUM_M60(c, m, d, x, ...) m(d, c-60, x) _ENUM_M59(c, m, d, __VA_ARGS__)
#define _ENUM_M61(c, m, d, x, ...) m(d, c-61, x) _ENUM_M60(c, m, d, __VA_ARGS__)
#define _ENUM_M62(c, m, d, x, ...) m(d, c-62, x) _ENUM_M61(c, m, d, __VA_ARGS__)
#define _ENUM_M63(c, m, d, x, ...) m(d, c-63, x) _ENUM_M62(c, m, d, __VA_ARGS__)
#define _ENUM_M64(c, m, d, x, ...) m(d, c-64, x) _ENUM_M63(c, m, d, __VA_ARGS__)
#define _ENUM_M65(c, m, d, x, ...) m(d, c-65, x) _ENUM_M64(c, m, d, __VA_ARGS__)
#define _ENUM_M66(c, m, d, x, ...) m(d, c-66, x) _ENUM_M65(c, m, d, __VA_ARGS__)
#define _ENUM_M67(c, m, d, x, ...) m(d, c-67, x) _ENUM_M66(c, m, d, __VA_ARGS__)
#define _ENUM_M68(c, m, d, x, ...) m(d, c-68, x) _ENUM_M67(c, m, d, __VA_ARGS__)
#define _ENUM_M69(c, m, d, x, ...) m(d, c-69, x) _ENUM_M68(c, m, d, __VA_ARGS__)
#define _ENUM_M70(c, m, d, x, ...) m(d, c-70, x) _ENUM_M69(c, m, d, __VA_ARGS__)
#define _ENUM_M71(c, m, d, x, ...) m(d, c-71, x) _ENUM_M70(c, m, d, __VA_ARGS__)
#define _ENUM_M72(c, m, d, x, ...) m(d, c-72, x) _ENUM_M71(c, m, d, __VA_ARGS__)
#define _ENUM_M73(c, m, d, x, ...) m(d, c-73, x) _ENUM_M72(c, m, d, __VA_ARGS__)
#define _ENUM_M74(c, m, d, x, ...) m(d, c-74, x) _ENUM_M73(c, m, d, __VA_ARGS__)
#define _ENUM_M75(c, m, d, x, ...) m(d, c-75, x) _ENUM_M74(c, m, d, __VA_ARGS__)
#define _ENUM_M76(c, m, d, x, ...) m(d, c-76, x) _ENUM_M75(c, m, d, __VA_ARGS__)
#define _ENUM_M77(c, m, d, x, ...) m(d, c-77, x) _ENUM_M76(c, m, d, __VA_ARGS__)
#define _ENUM_M78(c, m, d, x, ...) m(d, c-78, x) _ENUM_M77(c, m, d, __VA_ARGS__)
#define _ENUM_M79(c, m, d, x, ...) m(d, c-79, x) _ENUM_M78(c, m, d, __VA_ARGS__)
#define _ENUM_M80(c, m, d, x, ...) m(d, c-80, x) _ENUM_M79(c, m, d, __VA_ARGS__)
#define _ENUM_M81(c, m, d, x, ...) m(d, c-81, x) _ENUM_M80(c, m, d, __VA_ARGS__)
#define _ENUM_M82(c, m, d, x, ...) m(d, c-82, x) _ENUM_M81(c, m, d, __VA_ARGS__)
#define _ENUM_M83(c, m, d, x, ...) m(d, c-83, x) _ENUM_M82(c, m, d, __VA_ARGS__)
#define _ENUM_M84(c, m, d, x, ...) m(d, c-84, x) _ENUM_M83(c, m, d, __VA_ARGS__)
#define _ENUM_M85(c, m, d, x, ...) m(d, c-85, x) _ENUM_M84(c, m, d, __VA_ARGS__)
#define _ENUM_M86(c, m, d, x, ...) m(d, c-86, x) _ENUM_M85(c, m, d, __VA_ARGS__)
#define _ENUM_M87(c, m, d, x, ...) m(d, c-87, x) _ENUM_M86(c, m, d, __VA_ARGS__)
#define _ENUM_M88(c, m, d, x, ...) m(d, c-88, x) _ENUM_M87(c, m, d, __VA_ARGS__)
#define _ENUM_M89(c, m, d, x, ...) m(d, c-89, x) _ENUM_M88(c, m, d, __VA_ARGS__)
#define _ENUM_M90(c, m, d, x, ...) m(d, c-90, x) _ENUM_M89(c, m, d, __VA_ARGS__)
#define _ENUM_M91(c, m, d, x, ...) m(d, c-91, x) _ENUM_M90(c, m, d, __VA_ARGS__)
#define _ENUM_M92(c, m, d, x, ...) m(d, c-92, x) _ENUM_M91(c, m, d, __VA_ARGS__)
#define _ENUM_M93(c, m, d, x, ...) m(d, c-93, x) _ENUM_M92(c, m, d, __VA_ARGS__)
#define _ENUM_M94(c, m, d, x, ...) m(d, c-94, x) _ENUM_M93(c, m, d, __VA_ARGS__)
#define _ENUM_M95(c, m, d, x, ...) m(d, c-95, x) _ENUM_M94(c, m, d, __VA_ARGS__)
#define _ENUM_M96(c, m, d, x, ...) m(d, c-96, x) _ENUM_M95(c, m, d, __VA_ARGS__)
#define _ENUM_M97(c, m, d, x, ...) m(d, c-97, x) _ENUM_M96(c, m, d, __VA_ARGS__)
#define _ENUM_M98(c, m, d, x, ...) m(d, c-98, x) _ENUM_M97(c, m, d, __VA_ARGS__)
#define _ENUM_M99(c, m, d, x, ...) m(d, c-99, x) _ENUM_M98(c, m, d, __VA_ARGS__)
#define _ENUM_M100(c, m, d, x, ...) m(d, c-100, x) _ENUM_M99(c, m, d, __VA_ARGS__)
#define _ENUM_M101(c, m, d, x, ...) m(d, c-101, x) _ENUM_M100(c, m, d, __VA_ARGS__)
#define _ENUM_M102(c, m, d, x, ...) m(d, c-102, x) _ENUM_M101(c, m, d, __VA_ARGS__)
#define _ENUM_M103(c, m, d, x, ...) m(d, c-103, x) _ENUM_M102(c, m, d, __VA_ARGS__)
#define _ENUM_M104(c, m, d, x, ...) m(d, c-104, x) _ENUM_M103(c, m, d, __VA_ARGS__)
#define _ENUM_M105(c, m, d, x, ...) m(d, c-105, x) _ENUM_M104(c, m, d, __VA_ARGS__)
#define _ENUM_M106(c, m, d, x, ...) m(d, c-106, x) _ENUM_M105(c, m, d, __VA_ARGS__)
#define _ENUM_M107(c, m, d, x, ...) m(d, c-107, x) _ENUM_M106(c, m, d, __VA_ARGS__)
#define _ENUM_M108(c, m, d, x, ...) m(d, c-108, x) _ENUM_M107(c, m, d, __VA_ARGS__)
#define _ENUM_M109(c, m, d, x, ...) m(d, c-109, x) _ENUM_M108(c, m, d, __VA_ARGS__)
#define _ENUM_M110(c, m, d, x, ...) m(d, c-110, x) _ENUM_M109(c, m, d, __VA_ARGS__)
#define _ENUM_M111(c, m, d, x, ...) m(d, c-111, x) _ENUM_M110(c, m, d, __VA_ARGS__)
#define _ENUM_M112(c, m, d, x, ...) m(d, c-112, x) _ENUM_M111(c, m, d, __VA_ARGS__)
#define _ENUM_M113(c, m, d, x, ...) m(d, c-113, x) _ENUM_M112(c, m, d, __VA_ARGS__)
#define _ENUM_M114(c, m, d, x, ...) m(d, c-114, x) _ENUM_M113(c, m, d, __VA_ARGS__)
#define _ENUM_M115(c, m, d, x, ...) m(d, c-115, x) _ENUM_M114(c, m, d, __VA_ARGS__)
#define _ENUM_M116(c, m, d, x, ...) m(d, c-116, x) _ENUM_M115(c, m, d, __VA_ARGS__)
#define _ENUM_M117(c, m, d, x, ...) m(d, c-117, x) _ENUM_M116(c, m, d, __VA_ARGS__)
#define _ENUM_M118(c, m, d, x, ...) m(d, c-118, x) _ENUM_M117(c, m, d, __VA_ARGS__)
#define _ENUM_M119(c, m, d, x, ...) m(d, c-119, x) _ENUM_M118(c, m, d, __VA_ARGS__)
#define _ENUM_M120(c, m, d, x, ...) m(d, c-120, x) _ENUM_M119(c, m, d, __VA_ARGS__)
#define _ENUM_M121(c, m, d, x, ...) m(d, c-121, x) _ENUM_M120(c, m, d, __VA_ARGS__)
#define _ENUM_M122(c, m, d, x, ...) m(d, c-122, x) _ENUM_M121(c, m, d, __VA_ARGS__)
#define _ENUM_M123(c, m, d, x, ...) m(d, c-123, x) _ENUM_M122(c, m, d, __VA_ARGS__)
#define _ENUM_M124(c, m, d, x, ...) m(d, c-124, x) _ENUM_M123(c, m, d, __VA_ARGS__)
#define _ENUM_M125(c, m, d, x, ...) m(d, c-125, x) _ENUM_M124(c, m, d, __VA_ARGS__)
#define _ENUM_M126(c, m, d, x, ...) m(d, c-126, x) _ENUM_M125(c, m, d, __VA_ARGS__)
#define _ENUM_M127(c, m, d, x, ...) m(d, c-127, x) _ENUM_M126(c, m, d, __VA_ARGS__)
#define _ENUM_M128(c, m, d, x, ...) m(d, c-128, x) _ENUM_M127(c, m, d, __VA_ARGS__)
#define _ENUM_M129(c, m, d, x, ...) m(d, c-129, x) _ENUM_M128(c, m, d, __VA_ARGS__)
#define _ENUM_M130(c, m, d, x, ...) m(d, c-130, x) _ENUM_M129(c, m, d, __VA_ARGS__)
#define _ENUM_M131(c, m, d, x, ...) m(d, c-131, x) _ENUM_M130(c, m, d, __VA_ARGS__)
#define _ENUM_M132(c, m, d, x, ...) m(d, c-132, x) _ENUM_M131(c, m, d, __VA_ARGS__)
#define _ENUM_M133(c, m, d, x, ...) m(d, c-133, x) _ENUM_M132(c, m, d, __VA_ARGS__)
#define _ENUM_M134(c, m, d, x, ...) m(d, c-134, x) _ENUM_M133(c, m, d, __VA_ARGS__)
#define _ENUM_M135(c, m, d, x, ...) m(d, c-135, x) _ENUM_M134(c, m, d, __VA_ARGS__)
#define _ENUM_M136(c, m, d, x, ...) m(d, c-136, x) _ENUM_M135(c, m, d, __VA_ARGS__)
#define _ENUM_M137(c, m, d, x, ...) m(d, c-137, x) _ENUM_M136(c, m, d, __VA_ARGS__)
#define _ENUM_M138(c, m, d, x, ...) m(d, c-138, x) _ENUM_M137(c, m, d, __VA_ARGS__)
#define _ENUM_M139(c, m, d, x, ...) m(d, c-139, x) _ENUM_M138(c, m, d, __VA_ARGS__)
#define _ENUM_M140(c, m, d, x, ...) m(d, c-140, x) _ENUM_M139(c, m, d, __VA_ARGS__)
#define _ENUM_M141(c, m, d, x, ...) m(d, c-141, x) _ENUM_M140(c, m, d, __VA_ARGS__)
#define _ENUM_M142(c, m, d, x, ...) m(d, c-142, x) _ENUM_M141(c, m, d, __VA_ARGS__)
#define _ENUM_M143(c, m, d, x, ...) m(d, c-143, x) _ENUM_M142(c, m, d, __VA_ARGS__)
#define _ENUM_M144(c, m, d, x, ...) m(d, c-144, x) _ENUM_M143(c, m, d, __VA_ARGS__)
#define _ENUM_M145(c, m, d, x, ...) m(d, c-145, x) _ENUM_M144(c, m, d, __VA_ARGS__)
#define _ENUM_M146(c, m, d, x, ...) m(d, c-146, x) _ENUM_M145(c, m, d, __VA_ARGS__)
#define _ENUM_M147(c, m, d, x, ...) m(d, c-147, x) _ENUM_M146(c, m, d, __VA_ARGS__)
#define _ENUM_M148(c, m, d, x, ...) m(d, c-148, x) _ENUM_M147(c, m, d, __VA_ARGS__)
#define _ENUM_M149(c, m, d, x, ...) m(d, c-149, x) _ENUM_M148(c, m, d, __VA_ARGS__)
#define _ENUM_M150(c, m, d, x, ...) m(d, c-150, x) _ENUM_M149(c, m, d, __VA_ARGS__)
#define _ENUM_M151(c, m, d, x, ...) m(d, c-151, x) _ENUM_M150(c, m, d, __VA_ARGS__)
#define _ENUM_M152(c, m, d, x, ...) m(d, c-152, x) _ENUM_M151(c, m, d, __VA_ARGS__)
#define _ENUM_M153(c, m, d, x, ...) m(d, c-153, x) _ENUM_M152(c, m, d, __VA_ARGS__)
#define _ENUM_M154(c, m, d, x, ...) m(d, c-154, x) _ENUM_M153(c, m, d, __VA_ARGS__)
#define _ENUM_M155(c, m, d, x, ...) m(d, c-155, x) _ENUM_M154(c, m, d, __VA_ARGS__)
#define _ENUM_M156(c, m, d, x, ...) m(d, c-156, x) _ENUM_M155(c, m, d, __VA_ARGS__)
#define _ENUM_M157(c, m, d, x, ...) m(d, c-157, x) _ENUM_M156(c, m, d, __VA_ARGS__)
#define _ENUM_M158(c, m, d, x, ...) m(d, c-158, x) _ENUM_M157(c, m, d, __VA_ARGS__)
#define _ENUM_M159(c, m, d, x, ...) m(d, c-159, x) _ENUM_M158(c, m, d, __VA_ARGS__)
#define _ENUM_M160(c, m, d, x, ...) m(d, c-160, x) _ENUM_M159(c, m, d, __VA_ARGS__)
#define _ENUM_M161(c, m, d, x, ...) m(d, c-161, x) _ENUM_M160(c, m, d, __VA_ARGS__)
#define _ENUM_M162(c, m, d, x, ...) m(d, c-162, x) _ENUM_M161(c, m, d, __VA_ARGS__)
#define _ENUM_M163(c, m, d, x, ...) m(d, c-163, x) _ENUM_M162(c, m, d, __VA_ARGS__)
#define _ENUM_M164(c, m, d, x, ...) m(d, c-164, x) _ENUM_M163(c, m, d, __VA_ARGS__)
#define _ENUM_M165(c, m, d, x, ...) m(d, c-165, x) _ENUM_M164(c, m, d, __VA_ARGS__)
#define _ENUM_M166(c, m, d, x, ...) m(d, c-166, x) _ENUM_M165(c, m, d, __VA_ARGS__)
#define _ENUM_M167(c, m, d, x, ...) m(d, c-167, x) _ENUM_M166(c, m, d, __VA_ARGS__)
#define _ENUM_M168(c, m, d, x, ...) m(d, c-168, x) _ENUM_M167(c, m, d, __VA_ARGS__)
#define _ENUM_M169(c, m, d, x, ...) m(d, c-169, x) _ENUM_M168(c, m, d, __VA_ARGS__)
#define _ENUM_M170(c, m, d, x, ...) m(d, c-170, x) _ENUM_M169(c, m, d, __VA_ARGS__)
#define _ENUM_M171(c, m, d, x, ...) m(d, c-171, x) _ENUM_M170(c, m, d, __VA_ARGS__)
#define _ENUM_M172(c, m, d, x, ...) m(d, c-172, x) _ENUM_M171(c, m, d, __VA_ARGS__)
#define _ENUM_M173(c, m, d, x, ...) m(d, c-173, x) _ENUM_M172(c, m, d, __VA_ARGS__)
#define _ENUM_M174(c, m, d, x, ...) m(d, c-174, x) _ENUM_M173(c, m, d, __VA_ARGS__)
#define _ENUM_M175(c, m, d, x, ...) m(d, c-175, x) _ENUM_M174(c, m, d, __VA_ARGS__)
#define _ENUM_M176(c, m, d, x, ...) m(d, c-176, x) _ENUM_M175(c, m, d, __VA_ARGS__)
#define _ENUM_M177(c, m, d, x, ...) m(d, c-177, x) _ENUM_M176(c, m, d, __VA_ARGS__)
#define _ENUM_M178(c, m, d, x, ...) m(d, c-178, x) _ENUM_M177(c, m, d, __VA_ARGS__)
#define _ENUM_M179(c, m, d, x, ...) m(d, c-179, x) _ENUM_M178(c, m, d, __VA_ARGS__)
#define _ENUM_M180(c, m, d, x, ...) m(d, c-180, x) _ENUM_M179(c, m, d, __VA_ARGS__)
#define _ENUM_M181(c, m, d, x, ...) m(d, c-181, x) _ENUM_M180(c, m, d, __VA_ARGS__)
#define _ENUM_M182(c, m, d, x, ...) m(d, c-182, x) _ENUM_M181(c, m, d, __VA_ARGS__)
#define _ENUM_M183(c, m, d, x, ...) m(d, c-183, x) _ENUM_M182(c, m, d, __VA_ARGS__)
#define _ENUM_M184(c, m, d, x, ...) m(d, c-184, x) _ENUM_M183(c, m, d, __VA_ARGS__)
#define _ENUM_M185(c, m, d, x, ...) m(d, c-185, x) _ENUM_M184(c, m, d, __VA_ARGS__)
#define _ENUM_M186(c, m, d, x, ...) m(d, c-186, x) _ENUM_M185(c, m, d, __VA_ARGS__)
#define _ENUM_M187(c, m, d, x, ...) m(d, c-187, x) _ENUM_M186(c, m, d, __VA_ARGS__)
#define _ENUM_M188(c, m, d, x, ...) m(d, c-188, x) _ENUM_M187(c, m, d, __VA_ARGS__)
#define _ENUM_M189(c, m, d, x, ...) m(d, c-189, x) _ENUM_M188(c, m, d, __VA_ARGS__)
#define _ENUM_M190(c, m, d, x, ...) m(d, c-190, x) _ENUM_M189(c, m, d, __VA_ARGS__)
#define _ENUM_M191(c, m, d, x, ...) m(d, c-191, x) _ENUM_M190(c, m, d, __VA_ARGS__)
#define _ENUM_M192(c, m, d, x, ...) m(d, c-192, x) _ENUM_M191(c, m, d, __VA_ARGS__)
#define _ENUM_M193(c, m, d, x, ...) m(d, c-193, x) _ENUM_M192(c, m, d, __VA_ARGS__)
#define _ENUM_M194(c, m, d, x, ...) m(d, c-194, x) _ENUM_M193(c, m, d, __VA_ARGS__)
#define _ENUM_M195(c, m, d, x, ...) m(d, c-195, x) _ENUM_M194(c, m, d, __VA_ARGS__)
#define _ENUM_M196(c, m, d, x, ...) m(d, c-196, x) _ENUM_M195(c, m, d, __VA_ARGS__)
#define _ENUM_M197(c, m, d, x, ...) m(d, c-197, x) _ENUM_M196(c, m, d, __VA_ARGS__)
#define _ENUM_M198(c, m, d, x, ...) m(d, c-198, x) _ENUM_M197(c, m, d, __VA_ARGS__)
#define _ENUM_M199(c, m, d, x, ...) m(d, c-199, x) _ENUM_M198(c, m, d, __VA_ARGS__)
#define _ENUM_M200(c, m, d, x, ...) m(d, c-200, x) _ENUM_M199(c, m, d, __VA_ARGS__)
#define _ENUM_M201(c, m, d, x, ...) m(d, c-201, x) _ENUM_M200(c, m, d, __VA_ARGS__)
#define _ENUM_M202(c, m, d, x, ...) m(d, c-202, x) _ENUM_M201(c, m, d, __VA_ARGS__)
#define _ENUM_M203(c, m, d, x, ...) m(d, c-203, x) _ENUM_M202(c, m, d, __VA_ARGS__)
#define _ENUM_M204(c, m, d, x, ...) m(d, c-204, x) _ENUM_M203(c, m, d, __VA_ARGS__)
#define _ENUM_M205(c, m, d, x, ...) m(d, c-205, x) _ENUM_M204(c, m, d, __VA_ARGS__)
#define _ENUM_M206(c, m, d, x, ...) m(d, c-206, x) _ENUM_M205(c, m, d, __VA_ARGS__)
#define _ENUM_M207(c, m, d, x, ...) m(d, c-207, x) _ENUM_M206(c, m, d, __VA_ARGS__)
#define _ENUM_M208(c, m, d, x, ...) m(d, c-208, x) _ENUM_M207(c, m, d, __VA_ARGS__)
#define _ENUM_M209(c, m, d, x, ...) m(d, c-209, x) _ENUM_M208(c, m, d, __VA_ARGS__)
#define _ENUM_M210(c, m, d, x, ...) m(d, c-210, x) _ENUM_M209(c, m, d, __VA_ARGS__)
#define _ENUM_M211(c, m, d, x, ...) m(d, c-211, x) _ENUM_M210(c, m, d, __VA_ARGS__)
#define _ENUM_M212(c, m, d, x, ...) m(d, c-212, x) _ENUM_M211(c, m, d, __VA_ARGS__)
#define _ENUM_M213(c, m, d, x, ...) m(d, c-213, x) _ENUM_M212(c, m, d, __VA_ARGS__)
#define _ENUM_M214(c, m, d, x, ...) m(d, c-214, x) _ENUM_M213(c, m, d, __VA_ARGS__)
#define _ENUM_M215(c, m, d, x, ...) m(d, c-215, x) _ENUM_M214(c, m, d, __VA_ARGS__)
#define _ENUM_M216(c, m, d, x, ...) m(d, c-216, x) _ENUM_M215(c, m, d, __VA_ARGS__)
#define _ENUM_M217(c, m, d, x, ...) m(d, c-217, x) _ENUM_M216(c, m, d, __VA_ARGS__)
#define _ENUM_M218(c, m, d, x, ...) m(d, c-218, x) _ENUM_M217(c, m, d, __VA_ARGS__)
#define _ENUM_M219(c, m, d, x, ...) m(d, c-219, x) _ENUM_M218(c, m, d, __VA_ARGS__)
#define _ENUM_M220(c, m, d, x, ...) m(d, c-220, x) _ENUM_M219(c, m, d, __VA_ARGS__)
#define _ENUM_M221(c, m, d, x, ...) m(d, c-221, x) _ENUM_M220(c, m, d, __VA_ARGS__)
#define _ENUM_M222(c, m, d, x, ...) m(d, c-222, x) _ENUM_M221(c, m, d, __VA_ARGS__)
#define _ENUM_M223(c, m, d, x, ...) m(d, c-223, x) _ENUM_M222(c, m, d, __VA_ARGS__)
#define _ENUM_M224(c, m, d, x, ...) m(d, c-224, x) _ENUM_M223(c, m, d, __VA_ARGS__)
#define _ENUM_M225(c, m, d, x, ...) m(d, c-225, x) _ENUM_M224(c, m, d, __VA_ARGS__)
#define _ENUM_M226(c, m, d, x, ...) m(d, c-226, x) _ENUM_M225(c, m, d, __VA_ARGS__)
#define _ENUM_M227(c, m, d, x, ...) m(d, c-227, x) _ENUM_M226(c, m, d, __VA_ARGS__)
#define _ENUM_M228(c, m, d, x, ...) m(d, c-228, x) _ENUM_M227(c, m, d, __VA_ARGS__)
#define _ENUM_M229(c, m, d, x, ...) m(d, c-229, x) _ENUM_M228(c, m, d, __VA_ARGS__)
#define _ENUM_M230(c, m, d, x, ...) m(d, c-230, x) _ENUM_M229(c, m, d, __VA_ARGS__)
#define _ENUM_M231(c, m, d, x, ...) m(d, c-231, x) _ENUM_M230(c, m, d, __VA_ARGS__)
#define _ENUM_M232(c, m, d, x, ...) m(d, c-232, x) _ENUM_M231(c, m, d, __VA_ARGS__)
#define _ENUM_M233(c, m, d, x, ...) m(d, c-233, x) _ENUM_M232(c, m, d, __VA_ARGS__)
#define _ENUM_M234(c, m, d, x, ...) m(d, c-234, x) _ENUM_M233(c, m, d, __VA_ARGS__)
#define _ENUM_M235(c, m, d, x, ...) m(d, c-235, x) _ENUM_M234(c, m, d, __VA_ARGS__)
#define _ENUM_M236(c, m, d, x, ...) m(d, c-236, x) _ENUM_M235(c, m, d, __VA_ARGS__)
#define _ENUM_M237(c, m, d, x, ...) m(d, c-237, x) _ENUM_M236(c, m, d, __VA_ARGS__)
#define _ENUM_M238(c, m, d, x, ...) m(d, c-238, x) _ENUM_M237(c, m, d, __VA_ARGS__)
#define _ENUM_M239(c, m, d, x, ...) m(d, c-239, x) _ENUM_M238(c, m, d, __VA_ARGS__)
#define _ENUM_M240(c, m, d, x, ...) m(d, c-240, x) _ENUM_M239(c, m, d, __VA_ARGS__)
#define _ENUM_M241(c, m, d, x, ...) m(d, c-241, x) _ENUM_M240(c, m, d, __VA_ARGS__)
#define _ENUM_M242(c, m, d, x, ...) m(d, c-242, x) _ENUM_M241(c, m, d, __VA_ARGS__)
#define _ENUM_M243(c, m, d, x, ...) m(d, c-243, x) _ENUM_M242(c, m, d, __VA_ARGS__)
#define _ENUM_M244(c, m, d, x, ...) m(d, c-244, x) _ENUM_M243(c, m, d, __VA_ARGS__)
#define _ENUM_M245(c, m, d, x, ...) m(d, c-245, x) _ENUM_M244(c, m, d, __VA_ARGS__)
#define _ENUM_M246(c, m, d, x, ...) m(d, c-246, x) _ENUM_M245(c, m, d, __VA_ARGS__)
#define _ENUM_M247(c, m, d, x, ...) m(d, c-247, x) _ENUM_M246(c, m, d, __VA_ARGS__)
#define _ENUM_M248(c, m, d, x, ...) m(d, c-248, x) _ENUM_M247(c, m, d, __VA_ARGS__)
#define _ENUM_M249(c, m, d, x, ...) m(d, c-249, x) _ENUM_M248(c, m, d, __VA_ARGS__)
#define _ENUM_M250(c, m, d, x, ...) m(d, c-250, x) _ENUM_M249(c, m, d, __VA_ARGS__)
#define _ENUM_M251(c, m, d, x, ...) m(d, c-251, x) _ENUM_M250(c, m, d, __VA_ARGS__)
#define _ENUM_M252(c, m, d, x, ...) m(d, c-252, x) _ENUM_M251(c, m, d, __VA_ARGS__)
#define _ENUM_M253(c, m, d, x, ...) m(d, c-253, x) _ENUM_M252(c, m, d, __VA_ARGS__)
#define _ENUM_M254(c, m, d, x, ...) m(d, c-254, x) _ENUM_M253(c, m, d, __VA_ARGS__)
#define _ENUM_M255(c, m, d, x, ...) m(d, c-255, x) _ENUM_M254(c, m, d, __VA_ARGS__)
#define _ENUM_M256(c, m, d, x, ...) m(d, c-256, x) _ENUM_M255(c, m, d, __VA_ARGS__)
#define _ENUM_M257(c, m, d, x, ...) m(d, c-257, x) _ENUM_M256(c, m, d, __VA_ARGS__)
#define _ENUM_M258(c, m, d, x, ...) m(d, c-258, x) _ENUM_M257(c, m, d, __VA_ARGS__)
#define _ENUM_M259(c, m, d, x, ...) m(d, c-259, x) _ENUM_M258(c, m, d, __VA_ARGS__)
#define _ENUM_M260(c, m, d, x, ...) m(d, c-260, x) _ENUM_M259(c, m, d, __VA_ARGS__)
#define _ENUM_M261(c, m, d, x, ...) m(d, c-261, x) _ENUM_M260(c, m, d, __VA_ARGS__)
#define _ENUM_M262(c, m, d, x, ...) m(d, c-262, x) _ENUM_M261(c, m, d, __VA_ARGS__)
#define _ENUM_M263(c, m, d, x, ...) m(d, c-263, x) _ENUM_M262(c, m, d, __VA_ARGS__)
#define _ENUM_M264(c, m, d, x, ...) m(d, c-264, x) _ENUM_M263(c, m, d, __VA_ARGS__)
#define _ENUM_M265(c, m, d, x, ...) m(d, c-265, x) _ENUM_M264(c, m, d, __VA_ARGS__)
#define _ENUM_M266(c, m, d, x, ...) m(d, c-266, x) _ENUM_M265(c, m, d, __VA_ARGS__)
#define _ENUM_M267(c, m, d, x, ...) m(d, c-267, x) _ENUM_M266(c, m, d, __VA_ARGS__)
#define _ENUM_M268(c, m, d, x, ...) m(d, c-268, x) _ENUM_M267(c, m, d, __VA_ARGS__)
#define _ENUM_M269(c, m, d, x, ...) m(d, c-269, x) _ENUM_M268(c, m, d, __VA_ARGS__)
#define _ENUM_M270(c, m, d, x, ...) m(d, c-270, x) _ENUM_M269(c, m, d, __VA_ARGS__)
#define _ENUM_M271(c, m, d, x, ...) m(d, c-271, x) _ENUM_M270(c, m, d, __VA_ARGS__)
#define _ENUM_M272(c, m, d, x, ...) m(d, c-272, x) _ENUM_M271(c, m, d, __VA_ARGS__)
#define _ENUM_M273(c, m, d, x, ...) m(d, c-273, x) _ENUM_M272(c, m, d, __VA_ARGS__)
#define _ENUM_M274(c, m, d, x, ...) m(d, c-274, x) _ENUM_M273(c, m, d, __VA_ARGS__)
#define _ENUM_M275(c, m, d, x, ...) m(d, c-275, x) _ENUM_M274(c, m, d, __VA_ARGS__)
#define _ENUM_M276(c, m, d, x, ...) m(d, c-276, x) _ENUM_M275(c, m, d, __VA_ARGS__)
#define _ENUM_M277(c, m, d, x, ...) m(d, c-277, x) _ENUM_M276(c, m, d, __VA_ARGS__)
#define _ENUM_M278(c, m, d, x, ...) m(d, c-278, x) _ENUM_M277(c, m, d, __VA_ARGS__)
#define _ENUM_M279(c, m, d, x, ...) m(d, c-279, x) _ENUM_M278(c, m, d, __VA_ARGS__)
#define _ENUM_M280(c, m, d, x, ...) m(d, c-280, x) _ENUM_M279(c, m, d, __VA_ARGS__)
#define _ENUM_M281(c, m, d, x, ...) m(d, c-281, x) _ENUM_M280(c, m, d, __VA_ARGS__)
#define _ENUM_M282(c, m, d, x, ...) m(d, c-282, x) _ENUM_M281(c, m, d, __VA_ARGS__)
#define _ENUM_M283(c, m, d, x, ...) m(d, c-283, x) _ENUM_M282(c, m, d, __VA_ARGS__)
#define _ENUM_M284(c, m, d, x, ...) m(d, c-284, x) _ENUM_M283(c, m, d, __VA_ARGS__)
#define _ENUM_M285(c, m, d, x, ...) m(d, c-285, x) _ENUM_M284(c, m, d, __VA_ARGS__)
#define _ENUM_M286(c, m, d, x, ...) m(d, c-286, x) _ENUM_M285(c, m, d, __VA_ARGS__)
#define _ENUM_M287(c, m, d, x, ...) m(d, c-287, x) _ENUM_M286(c, m, d, __VA_ARGS__)
#define _ENUM_M288(c, m, d, x, ...) m(d, c-288, x) _ENUM_M287(c, m, d, __VA_ARGS__)
#define _ENUM_M289(c, m, d, x, ...) m(d, c-289, x) _ENUM_M288(c, m, d, __VA_ARGS__)
#define _ENUM_M290(c, m, d, x, ...) m(d, c-290, x) _ENUM_M289(c, m, d, __VA_ARGS__)
#define _ENUM_M291(c, m, d, x, ...) m(d, c-291, x) _ENUM_M290(c, m, d, __VA_ARGS__)
#define _ENUM_M292(c, m, d, x, ...) m(d, c-292, x) _ENUM_M291(c, m, d, __VA_ARGS__)
#define _ENUM_M293(c, m, d, x, ...) m(d, c-293, x) _ENUM_M292(c, m, d, __VA_ARGS__)
#define _ENUM_M294(c, m, d, x, ...) m(d, c-294, x) _ENUM_M293(c, m, d, __VA_ARGS__)
#define _ENUM_M295(c, m, d, x, ...) m(d, c-295, x) _ENUM_M294(c, m, d, __VA_ARGS__)
#define _ENUM_M296(c, m, d, x, ...) m(d, c-296, x) _ENUM_M295(c, m, d, __VA_ARGS__)
#define _ENUM_M297(c, m, d, x, ...) m(d, c-297, x) _ENUM_M296(c, m, d, __VA_ARGS__)
#define _ENUM_M298(c, m, d, x, ...) m(d, c-298, x) _ENUM_M297(c, m, d, __VA_ARGS__)
#define _ENUM_M299(c, m, d, x, ...) m(d, c-299, x) _ENUM_M298(c, m, d, __VA_ARGS__)
#define _ENUM_M300(c, m, d, x, ...) m(d, c-300, x) _ENUM_M299(c, m, d, __VA_ARGS__)
#define _ENUM_M301(c, m, d, x, ...) m(d, c-301, x) _ENUM_M300(c, m, d, __VA_ARGS__)
#define _ENUM_M302(c, m, d, x, ...) m(d, c-302, x) _ENUM_M301(c, m, d, __VA_ARGS__)
#define _ENUM_M303(c, m, d, x, ...) m(d, c-303, x) _ENUM_M302(c, m, d, __VA_ARGS__)
#define _ENUM_M304(c, m, d, x, ...) m(d, c-304, x) _ENUM_M303(c, m, d, __VA_ARGS__)
#define _ENUM_M305(c, m, d, x, ...) m(d, c-305, x) _ENUM_M304(c, m, d, __VA_ARGS__)
#define _ENUM_M306(c, m, d, x, ...) m(d, c-306, x) _ENUM_M305(c, m, d, __VA_ARGS__)
#define _ENUM_M307(c, m, d, x, ...) m(d, c-307, x) _ENUM_M306(c, m, d, __VA_ARGS__)
#define _ENUM_M308(c, m, d, x, ...) m(d, c-308, x) _ENUM_M307(c, m, d, __VA_ARGS__)
#define _ENUM_M309(c, m, d, x, ...) m(d, c-309, x) _ENUM_M308(c, m, d, __VA_ARGS__)
#define _ENUM_M310(c, m, d, x, ...) m(d, c-310, x) _ENUM_M309(c, m, d, __VA_ARGS__)
#define _ENUM_M311(c, m, d, x, ...) m(d, c-311, x) _ENUM_M310(c, m, d, __VA_ARGS__)
#define _ENUM_M312(c, m, d, x, ...) m(d, c-312, x) _ENUM_M311(c, m, d, __VA_ARGS__)
#define _ENUM_M313(c, m, d, x, ...) m(d, c-313, x) _ENUM_M312(c, m, d, __VA_ARGS__)
#define _ENUM_M314(c, m, d, x, ...) m(d, c-314, x) _ENUM_M313(c, m, d, __VA_ARGS__)
#define _ENUM_M315(c, m, d, x, ...) m(d, c-315, x) _ENUM_M314(c, m, d, __VA_ARGS__)
#define _ENUM_M316(c, m, d, x, ...) m(d, c-316, x) _ENUM_M315(c, m, d, __VA_ARGS__)
#define _ENUM_M317(c, m, d, x, ...) m(d, c-317, x) _ENUM_M316(c, m, d, __VA_ARGS__)
#define _ENUM_M318(c, m, d, x, ...) m(d, c-318, x) _ENUM_M317(c, m, d, __VA_ARGS__)
#define _ENUM_M319(c, m, d, x, ...) m(d, c-319, x) _ENUM_M318(c, m, d, __VA_ARGS__)
#define _ENUM_M320(c, m, d, x, ...) m(d, c-320, x) _ENUM_M319(c, m, d, __VA_ARGS__)
#define _ENUM_M321(c, m, d, x, ...) m(d, c-321, x) _ENUM_M320(c, m, d, __VA_ARGS__)
#define _ENUM_M322(c, m, d, x, ...) m(d, c-322, x) _ENUM_M321(c, m, d, __VA_ARGS__)
#define _ENUM_M323(c, m, d, x, ...) m(d, c-323, x) _ENUM_M322(c, m, d, __VA_ARGS__)
#define _ENUM_M324(c, m, d, x, ...) m(d, c-324, x) _ENUM_M323(c, m, d, __VA_ARGS__)
#define _ENUM_M325(c, m, d, x, ...) m(d, c-325, x) _ENUM_M324(c, m, d, __VA_ARGS__)
#define _ENUM_M326(c, m, d, x, ...) m(d, c-326, x) _ENUM_M325(c, m, d, __VA_ARGS__)
#define _ENUM_M327(c, m, d, x, ...) m(d, c-327, x) _ENUM_M326(c, m, d, __VA_ARGS__)
#define _ENUM_M328(c, m, d, x, ...) m(d, c-328, x) _ENUM_M327(c, m, d, __VA_ARGS__)
#define _ENUM_M329(c, m, d, x, ...) m(d, c-329, x) _ENUM_M328(c, m, d, __VA_ARGS__)
#define _ENUM_M330(c, m, d, x, ...) m(d, c-330, x) _ENUM_M329(c, m, d, __VA_ARGS__)
#define _ENUM_M331(c, m, d, x, ...) m(d, c-331, x) _ENUM_M330(c, m, d, __VA_ARGS__)
#define _ENUM_M332(c, m, d, x, ...) m(d, c-332, x) _ENUM_M331(c, m, d, __VA_ARGS__)
#define _ENUM_M333(c, m, d, x, ...) m(d, c-333, x) _ENUM_M332(c, m, d, __VA_ARGS__)
#define _ENUM_M334(c, m, d, x, ...) m(d, c-334, x) _ENUM_M333(c, m, d, __VA_ARGS__)
#define _ENUM_M335(c, m, d, x, ...) m(d, c-335, x) _ENUM_M334(c, m, d, __VA_ARGS__)
#define _ENUM_M336(c, m, d, x, ...) m(d, c-336, x) _ENUM_M335(c, m, d, __VA_ARGS__)
#define _ENUM_M337(c, m, d, x, ...) m(d, c-337, x) _ENUM_M336(c, m, d, __VA_ARGS__)
#define _ENUM_M338(c, m, d, x, ...) m(d, c-338, x) _ENUM_M337(c, m, d, __VA_ARGS__)
#define _ENUM_M339(c, m, d, x, ...) m(d, c-339, x) _ENUM_M338(c, m, d, __VA_ARGS__)
#define _ENUM_M340(c, m, d, x, ...) m(d, c-340, x) _ENUM_M339(c, m, d, __VA_ARGS__)
#define _ENUM_M341(c, m, d, x, ...) m(d, c-341, x) _ENUM_M340(c, m, d, __VA_ARGS__)
#define _ENUM_M342(c, m, d, x, ...) m(d, c-342, x) _ENUM_M341(c, m, d, __VA_ARGS__)
#define _ENUM_M343(c, m, d, x, ...) m(d, c-343, x) _ENUM_M342(c, m, d, __VA_ARGS__)
#define _ENUM_M344(c, m, d, x, ...) m(d, c-344, x) _ENUM_M343(c, m, d, __VA_ARGS__)
#define _ENUM_M345(c, m, d, x, ...) m(d, c-345, x) _ENUM_M344(c, m, d, __VA_ARGS__)
#define _ENUM_M346(c, m, d, x, ...) m(d, c-346, x) _ENUM_M345(c, m, d, __VA_ARGS__)
#define _ENUM_M347(c, m, d, x, ...) m(d, c-347, x) _ENUM_M346(c, m, d, __VA_ARGS__)
#define _ENUM_M348(c, m, d, x, ...) m(d, c-348, x) _ENUM_M347(c, m, d, __VA_ARGS__)
#define _ENUM_M349(c, m, d, x, ...) m(d, c-349, x) _ENUM_M348(c, m, d, __VA_ARGS__)
#define _ENUM_M350(c, m, d, x, ...) m(d, c-350, x) _ENUM_M349(c, m, d, __VA_ARGS__)
#define _ENUM_M351(c, m, d, x, ...) m(d, c-351, x) _ENUM_M350(c, m, d, __VA_ARGS__)
#define _ENUM_M352(c, m, d, x, ...) m(d, c-352, x) _ENUM_M351(c, m, d, __VA_ARGS__)
#define _ENUM_M353(c, m, d, x, ...) m(d, c-353, x) _ENUM_M352(c, m, d, __VA_ARGS__)
#define _ENUM_M354(c, m, d, x, ...) m(d, c-354, x) _ENUM_M353(c, m, d, __VA_ARGS__)
#define _ENUM_M355(c, m, d, x, ...) m(d, c-355, x) _ENUM_M354(c, m, d, __VA_ARGS__)
#define _ENUM_M356(c, m, d, x, ...) m(d, c-356, x) _ENUM_M355(c, m, d, __VA_ARGS__)
#define _ENUM_M357(c, m, d, x, ...) m(d, c-357, x) _ENUM_M356(c, m, d, __VA_ARGS__)
#define _ENUM_M358(c, m, d, x, ...) m(d, c-358, x) _ENUM_M357(c, m, d, __VA_ARGS__)
#define _ENUM_M359(c, m, d, x, ...) m(d, c-359, x) _ENUM_M358(c, m, d, __VA_ARGS__)
#define _ENUM_M360(c, m, d, x, ...) m(d, c-360, x) _ENUM_M359(c, m, d, __VA_ARGS__)
#define _ENUM_M361(c, m, d, x, ...) m(d, c-361, x) _ENUM_M360(c, m, d, __VA_ARGS__)
#define _ENUM_M362(c, m, d, x, ...) m(d, c-362, x) _ENUM_M361(c, m, d, __VA_ARGS__)
#define _ENUM_M363(c, m, d, x, ...) m(d, c-363, x) _ENUM_M362(c, m, d, __VA_ARGS__)
#define _ENUM_M364(c, m, d, x, ...) m(d, c-364, x) _ENUM_M363(c, m, d, __VA_ARGS__)
#define _ENUM_M365(c, m, d, x, ...) m(d, c-365, x) _ENUM_M364(c, m, d, __VA_ARGS__)
#define _ENUM_M366(c, m, d, x, ...) m(d, c-366, x) _ENUM_M365(c, m, d, __VA_ARGS__)
#define _ENUM_M367(c, m, d, x, ...) m(d, c-367, x) _ENUM_M366(c, m, d, __VA_ARGS__)
#define _ENUM_M368(c, m, d, x, ...) m(d, c-368, x) _ENUM_M367(c, m, d, __VA_ARGS__)
#define _ENUM_M369(c, m, d, x, ...) m(d, c-369, x) _ENUM_M368(c, m, d, __VA_ARGS__)
#define _ENUM_M370(c, m, d, x, ...) m(d, c-370, x) _ENUM_M369(c, m, d, __VA_ARGS__)
#define _ENUM_M371(c, m, d, x, ...) m(d, c-371, x) _ENUM_M370(c, m, d, __VA_ARGS__)
#define _ENUM_M372(c, m, d, x, ...) m(d, c-372, x) _ENUM_M371(c, m, d, __VA_ARGS__)
#define _ENUM_M373(c, m, d, x, ...) m(d, c-373, x) _ENUM_M372(c, m, d, __VA_ARGS__)
#define _ENUM_M374(c, m, d, x, ...) m(d, c-374, x) _ENUM_M373(c, m, d, __VA_ARGS__)
#define _ENUM_M375(c, m, d, x, ...) m(d, c-375, x) _ENUM_M374(c, m, d, __VA_ARGS__)
#define _ENUM_M376(c, m, d, x, ...) m(d, c-376, x) _ENUM_M375(c, m, d, __VA_ARGS__)
#define _ENUM_M377(c, m, d, x, ...) m(d, c-377, x) _ENUM_M376(c, m, d, __VA_ARGS__)
#define _ENUM_M378(c, m, d, x, ...) m(d, c-378, x) _ENUM_M377(c, m, d, __VA_ARGS__)
#define _ENUM_M379(c, m, d, x, ...) m(d, c-379, x) _ENUM_M378(c, m, d, __VA_ARGS__)
#define _ENUM_M380(c, m, d, x, ...) m(d, c-380, x) _ENUM_M379(c, m, d, __VA_ARGS__)
#define _ENUM_M381(c, m, d, x, ...) m(d, c-381, x) _ENUM_M380(c, m, d, __VA_ARGS__)
#define _ENUM_M382(c, m, d, x, ...) m(d, c-382, x) _ENUM_M381(c, m, d, __VA_ARGS__)
#define _ENUM_M383(c, m, d, x, ...) m(d, c-383, x) _ENUM_M382(c, m, d, __VA_ARGS__)
#define _ENUM_M384(c, m, d, x, ...) m(d, c-384, x) _ENUM_M383(c, m, d, __VA_ARGS__)
#define _ENUM_M385(c, m, d, x, ...) m(d, c-385, x) _ENUM_M384(c, m, d, __VA_ARGS__)
#define _ENUM_M386(c, m, d, x, ...) m(d, c-386, x) _ENUM_M385(c, m, d, __VA_ARGS__)
#define _ENUM_M387(c, m, d, x, ...) m(d, c-387, x) _ENUM_M386(c, m, d, __VA_ARGS__)
#define _ENUM_M388(c, m, d, x, ...) m(d, c-388, x) _ENUM_M387(c, m, d, __VA_ARGS__)
#define _ENUM_M389(c, m, d, x, ...) m(d, c-389, x) _ENUM_M388(c, m, d, __VA_ARGS__)
#define _ENUM_M390(c, m, d, x, ...) m(d, c-390, x) _ENUM_M389(c, m, d, __VA_ARGS__)
#define _ENUM_M391(c, m, d, x, ...) m(d, c-391, x) _ENUM_M390(c, m, d, __VA_ARGS__)
#define _ENUM_M392(c, m, d, x, ...) m(d, c-392, x) _ENUM_M391(c, m, d, __VA_ARGS__)
#define _ENUM_M393(c, m, d, x, ...) m(d, c-393, x) _ENUM_M392(c, m, d, __VA_ARGS__)
#define _ENUM_M394(c, m, d, x, ...) m(d, c-394, x) _ENUM_M393(c, m, d, __VA_ARGS__)
#define _ENUM_M395(c, m, d, x, ...) m(d, c-395, x) _ENUM_M394(c, m, d, __VA_ARGS__)
#define _ENUM_M396(c, m, d, x, ...) m(d, c-396, x) _ENUM_M395(c, m, d, __VA_ARGS__)
#define _ENUM_M397(c, m, d, x, ...) m(d, c-397, x) _ENUM_M396(c, m, d, __VA_ARGS__)
#define _ENUM_M398(c, m, d, x, ...) m(d, c-398, x) _ENUM_M397(c, m, d, __VA_ARGS__)
#define _ENUM_M399(c, m, d, x, ...) m(d, c-399, x) _ENUM_M398(c, m, d, __VA_ARGS__)
#define _ENUM_M400(c, m, d, x, ...) m(d, c-400, x) _ENUM_M399(c, m, d, __VA_ARGS__)
#define _ENUM_M401(c, m, d, x, ...) m(d, c-401, x) _ENUM_M400(c, m, d, __VA_ARGS__)
#define _ENUM_M402(c, m, d, x, ...) m(d, c-402, x) _ENUM_M401(c, m, d, __VA_ARGS__)
#define _ENUM_M403(c, m, d, x, ...) m(d, c-403, x) _ENUM_M402(c, m, d, __VA_ARGS__)
#define _ENUM_M404(c, m, d, x, ...) m(d, c-404, x) _ENUM_M403(c, m, d, __VA_ARGS__)
#define _ENUM_M405(c, m, d, x, ...) m(d, c-405, x) _ENUM_M404(c, m, d, __VA_ARGS__)
#define _ENUM_M406(c, m, d, x, ...) m(d, c-406, x) _ENUM_M405(c, m, d, __VA_ARGS__)
#define _ENUM_M407(c, m, d, x, ...) m(d, c-407, x) _ENUM_M406(c, m, d, __VA_ARGS__)
#define _ENUM_M408(c, m, d, x, ...) m(d, c-408, x) _ENUM_M407(c, m, d, __VA_ARGS__)
#define _ENUM_M409(c, m, d, x, ...) m(d, c-409, x) _ENUM_M408(c, m, d, __VA_ARGS__)
#define _ENUM_M410(c, m, d, x, ...) m(d, c-410, x) _ENUM_M409(c, m, d, __VA_ARGS__)
#define _ENUM_M411(c, m, d, x, ...) m(d, c-411, x) _ENUM_M410(c, m, d, __VA_ARGS__)
#define _ENUM_M412(c, m, d, x, ...) m(d, c-412, x) _ENUM_M411(c, m, d, __VA_ARGS__)
#define _ENUM_M413(c, m, d, x, ...) m(d, c-413, x) _ENUM_M412(c, m, d, __VA_ARGS__)
#define _ENUM_M414(c, m, d, x, ...) m(d, c-414, x) _ENUM_M413(c, m, d, __VA_ARGS__)
#define _ENUM_M415(c, m, d, x, ...) m(d, c-415, x) _ENUM_M414(c, m, d, __VA_ARGS__)
#define _ENUM_M416(c, m, d, x, ...) m(d, c-416, x) _ENUM_M415(c, m, d, __VA_ARGS__)
#define _ENUM_M417(c, m, d, x, ...) m(d, c-417, x) _ENUM_M416(c, m, d, __VA_ARGS__)
#define _ENUM_M418(c, m, d, x, ...) m(d, c-418, x) _ENUM_M417(c, m, d, __VA_ARGS__)
#define _ENUM_M419(c, m, d, x, ...) m(d, c-419, x) _ENUM_M418(c, m, d, __VA_ARGS__)
#define _ENUM_M420(c, m, d, x, ...) m(d, c-420, x) _ENUM_M419(c, m, d, __VA_ARGS__)
#define _ENUM_M421(c, m, d, x, ...) m(d, c-421, x) _ENUM_M420(c, m, d, __VA_ARGS__)
#define _ENUM_M422(c, m, d, x, ...) m(d, c-422, x) _ENUM_M421(c, m, d, __VA_ARGS__)
#define _ENUM_M423(c, m, d, x, ...) m(d, c-423, x) _ENUM_M422(c, m, d, __VA_ARGS__)
#define _ENUM_M424(c, m, d, x, ...) m(d, c-424, x) _ENUM_M423(c, m, d, __VA_ARGS__)
#define _ENUM_M425(c, m, d, x, ...) m(d, c-425, x) _ENUM_M424(c, m, d, __VA_ARGS__)
#define _ENUM_M426(c, m, d, x, ...) m(d, c-426, x) _ENUM_M425(c, m, d, __VA_ARGS__)
#define _ENUM_M427(c, m, d, x, ...) m(d, c-427, x) _ENUM_M426(c, m, d, __VA_ARGS__)
#define _ENUM_M428(c, m, d, x, ...) m(d, c-428, x) _ENUM_M427(c, m, d, __VA_ARGS__)
#define _ENUM_M429(c, m, d, x, ...) m(d, c-429, x) _ENUM_M428(c, m, d, __VA_ARGS__)
#define _ENUM_M430(c, m, d, x, ...) m(d, c-430, x) _ENUM_M429(c, m, d, __VA_ARGS__)
#define _ENUM_M431(c, m, d, x, ...) m(d, c-431, x) _ENUM_M430(c, m, d, __VA_ARGS__)
#define _ENUM_M432(c, m, d, x, ...) m(d, c-432, x) _ENUM_M431(c, m, d, __VA_ARGS__)
#define _ENUM_M433(c, m, d, x, ...) m(d, c-433, x) _ENUM_M432(c, m, d, __VA_ARGS__)
#define _ENUM_M434(c, m, d, x, ...) m(d, c-434, x) _ENUM_M433(c, m, d, __VA_ARGS__)
#define _ENUM_M435(c, m, d, x, ...) m(d, c-435, x) _ENUM_M434(c, m, d, __VA_ARGS__)
#define _ENUM_M436(c, m, d, x, ...) m(d, c-436, x) _ENUM_M435(c, m, d, __VA_ARGS__)
#define _ENUM_M437(c, m, d, x, ...) m(d, c-437, x) _ENUM_M436(c, m, d, __VA_ARGS__)
#define _ENUM_M438(c, m, d, x, ...) m(d, c-438, x) _ENUM_M437(c, m, d, __VA_ARGS__)
#define _ENUM_M439(c, m, d, x, ...) m(d, c-439, x) _ENUM_M438(c, m, d, __VA_ARGS__)
#define _ENUM_M440(c, m, d, x, ...) m(d, c-440, x) _ENUM_M439(c, m, d, __VA_ARGS__)
#define _ENUM_M441(c, m, d, x, ...) m(d, c-441, x) _ENUM_M440(c, m, d, __VA_ARGS__)
#define _ENUM_M442(c, m, d, x, ...) m(d, c-442, x) _ENUM_M441(c, m, d, __VA_ARGS__)
#define _ENUM_M443(c, m, d, x, ...) m(d, c-443, x) _ENUM_M442(c, m, d, __VA_ARGS__)
#define _ENUM_M444(c, m, d, x, ...) m(d, c-444, x) _ENUM_M443(c, m, d, __VA_ARGS__)
#define _ENUM_M445(c, m, d, x, ...) m(d, c-445, x) _ENUM_M444(c, m, d, __VA_ARGS__)
#define _ENUM_M446(c, m, d, x, ...) m(d, c-446, x) _ENUM_M445(c, m, d, __VA_ARGS__)
#define _ENUM_M447(c, m, d, x, ...) m(d, c-447, x) _ENUM_M446(c, m, d, __VA_ARGS__)
#define _ENUM_M448(c, m, d, x, ...) m(d, c-448, x) _ENUM_M447(c, m, d, __VA_ARGS__)
#define _ENUM_M449(c, m, d, x, ...) m(d, c-449, x) _ENUM_M448(c, m, d, __VA_ARGS__)
#define _ENUM_M450(c, m, d, x, ...) m(d, c-450, x) _ENUM_M449(c, m, d, __VA_ARGS__)
#define _ENUM_M451(c, m, d, x, ...) m(d, c-451, x) _ENUM_M450(c, m, d, __VA_ARGS__)
#define _ENUM_M452(c, m, d, x, ...) m(d, c-452, x) _ENUM_M451(c, m, d, __VA_ARGS__)
#define _ENUM_M453(c, m, d, x, ...) m(d, c-453, x) _ENUM_M452(c, m, d, __VA_ARGS__)
#define _ENUM_M454(c, m, d, x, ...) m(d, c-454, x) _ENUM_M453(c, m, d, __VA_ARGS__)
#define _ENUM_M455(c, m, d, x, ...) m(d, c-455, x) _ENUM_M454(c, m, d, __VA_ARGS__)
#define _ENUM_M456(c, m, d, x, ...) m(d, c-456, x) _ENUM_M455(c, m, d, __VA_ARGS__)
#define _ENUM_M457(c, m, d, x, ...) m(d, c-457, x) _ENUM_M456(c, m, d, __VA_ARGS__)
#define _ENUM_M458(c, m, d, x, ...) m(d, c-458, x) _ENUM_M457(c, m, d, __VA_ARGS__)
#define _ENUM_M459(c, m, d, x, ...) m(d, c-459, x) _ENUM_M458(c, m, d, __VA_ARGS__)
#define _ENUM_M460(c, m, d, x, ...) m(d, c-460, x) _ENUM_M459(c, m, d, __VA_ARGS__)
#define _ENUM_M461(c, m, d, x, ...) m(d, c-461, x) _ENUM_M460(c, m, d, __VA_ARGS__)
#define _ENUM_M462(c, m, d, x, ...) m(d, c-462, x) _ENUM_M461(c, m, d, __VA_ARGS__)
#define _ENUM_M463(c, m, d, x, ...) m(d, c-463, x) _ENUM_M462(c, m, d, __VA_ARGS__)
#define _ENUM_M464(c, m, d, x, ...) m(d, c-464, x) _ENUM_M463(c, m, d, __VA_ARGS__)
#define _ENUM_M465(c, m, d, x, ...) m(d, c-465, x) _ENUM_M464(c, m, d, __VA_ARGS__)
#define _ENUM_M466(c, m, d, x, ...) m(d, c-466, x) _ENUM_M465(c, m, d, __VA_ARGS__)
#define _ENUM_M467(c, m, d, x, ...) m(d, c-467, x) _ENUM_M466(c, m, d, __VA_ARGS__)
#define _ENUM_M468(c, m, d, x, ...) m(d, c-468, x) _ENUM_M467(c, m, d, __VA_ARGS__)
#define _ENUM_M469(c, m, d, x, ...) m(d, c-469, x) _ENUM_M468(c, m, d, __VA_ARGS__)
#define _ENUM_M470(c, m, d, x, ...) m(d, c-470, x) _ENUM_M469(c, m, d, __VA_ARGS__)
#define _ENUM_M471(c, m, d, x, ...) m(d, c-471, x) _ENUM_M470(c, m, d, __VA_ARGS__)
#define _ENUM_M472(c, m, d, x, ...) m(d, c-472, x) _ENUM_M471(c, m, d, __VA_ARGS__)
#define _ENUM_M473(c, m, d, x, ...) m(d, c-473, x) _ENUM_M472(c, m, d, __VA_ARGS__)
#define _ENUM_M474(c, m, d, x, ...) m(d, c-474, x) _ENUM_M473(c, m, d, __VA_ARGS__)
#define _ENUM_M475(c, m, d, x, ...) m(d, c-475, x) _ENUM_M474(c, m, d, __VA_ARGS__)
#define _ENUM_M476(c, m, d, x, ...) m(d, c-476, x) _ENUM_M475(c, m, d, __VA_ARGS__)
#define _ENUM_M477(c, m, d, x, ...) m(d, c-477, x) _ENUM_M476(c, m, d, __VA_ARGS__)
#define _ENUM_M478(c, m, d, x, ...) m(d, c-478, x) _ENUM_M477(c, m, d, __VA_ARGS__)
#define _ENUM_M479(c, m, d, x, ...) m(d, c-479, x) _ENUM_M478(c, m, d, __VA_ARGS__)
#define _ENUM_M480(c, m, d, x, ...) m(d, c-480, x) _ENUM_M479(c, m, d, __VA_ARGS__)
#define _ENUM_M481(c, m, d, x, ...) m(d, c-481, x) _ENUM_M480(c, m, d, __VA_ARGS__)
#define _ENUM_M482(c, m, d, x, ...) m(d, c-482, x) _ENUM_M481(c, m, d, __VA_ARGS__)
#define _ENUM_M483(c, m, d, x, ...) m(d, c-483, x) _ENUM_M482(c, m, d, __VA_ARGS__)
#define _ENUM_M484(c, m, d, x, ...) m(d, c-484, x) _ENUM_M483(c, m, d, __VA_ARGS__)
#define _ENUM_M485(c, m, d, x, ...) m(d, c-485, x) _ENUM_M484(c, m, d, __VA_ARGS__)
#define _ENUM_M486(c, m, d, x, ...) m(d, c-486, x) _ENUM_M485(c, m, d, __VA_ARGS__)
#define _ENUM_M487(c, m, d, x, ...) m(d, c-487, x) _ENUM_M486(c, m, d, __VA_ARGS__)
#define _ENUM_M488(c, m, d, x, ...) m(d, c-488, x) _ENUM_M487(c, m, d, __VA_ARGS__)
#define _ENUM_M489(c, m, d, x, ...) m(d, c-489, x) _ENUM_M488(c, m, d, __VA_ARGS__)
#define _ENUM_M490(c, m, d, x, ...) m(d, c-490, x) _ENUM_M489(c, m, d, __VA_ARGS__)
#define _ENUM_M491(c, m, d, x, ...) m(d, c-491, x) _ENUM_M490(c, m, d, __VA_ARGS__)
#define _ENUM_M492(c, m, d, x, ...) m(d, c-492, x) _ENUM_M491(c, m, d, __VA_ARGS__)
#define _ENUM_M493(c, m, d, x, ...) m(d, c-493, x) _ENUM_M492(c, m, d, __VA_ARGS__)
#define _ENUM_M494(c, m, d, x, ...) m(d, c-494, x) _ENUM_M493(c, m, d, __VA_ARGS__)
#define _ENUM_M495(c, m, d, x, ...) m(d, c-495, x) _ENUM_M494(c, m, d, __VA_ARGS__)
#define _ENUM_M496(c, m, d, x, ...) m(d, c-496, x) _ENUM_M495(c, m, d, __VA_ARGS__)
#define _ENUM_M497(c, m, d, x, ...) m(d, c-497, x) _ENUM_M496(c, m, d, __VA_ARGS__)
#define _ENUM_M498(c, m, d, x, ...) m(d, c-498, x) _ENUM_M497(c, m, d, __VA_ARGS__)
#define _ENUM_M499(c, m, d, x, ...) m(d, c-499, x) _ENUM_M498(c, m, d, __VA_ARGS__)
#define _ENUM_M500(c, m, d, x, ...) m(d, c-500, x) _ENUM_M499(c, m, d, __VA_ARGS__)
#define _ENUM_M501(c, m, d, x, ...) m(d, c-501, x) _ENUM_M500(c, m, d, __VA_ARGS__)
#define _ENUM_M502(c, m, d, x, ...) m(d, c-502, x) _ENUM_M501(c, m, d, __VA_ARGS__)
#define _ENUM_M503(c, m, d, x, ...) m(d, c-503, x) _ENUM_M502(c, m, d, __VA_ARGS__)
#define _ENUM_M504(c, m, d, x, ...) m(d, c-504, x) _ENUM_M503(c, m, d, __VA_ARGS__)
#define _ENUM_M505(c, m, d, x, ...) m(d, c-505, x) _ENUM_M504(c, m, d, __VA_ARGS__)
#define _ENUM_M506(c, m, d, x, ...) m(d, c-506, x) _ENUM_M505(c, m, d, __VA_ARGS__)
#define _ENUM_M507(c, m, d, x, ...) m(d, c-507, x) _ENUM_M506(c, m, d, __VA_ARGS__)
#define _ENUM_M508(c, m, d, x, ...) m(d, c-508, x) _ENUM_M507(c, m, d, __VA_ARGS__)
#define _ENUM_M509(c, m, d, x, ...) m(d, c-509, x) _ENUM_M508(c, m, d, __VA_ARGS__)
#define _ENUM_M510(c, m, d, x, ...) m(d, c-510, x) _ENUM_M509(c, m, d, __VA_ARGS__)
#define _ENUM_M511(c, m, d, x, ...) m(d, c-511, x) _ENUM_M510(c, m, d, __VA_ARGS__)
#define _ENUM_M512(c, m, d, x, ...) m(d, c-512, x) _ENUM_M511(c, m, d, __VA_ARGS__)
#define _ENUM_M513(c, m, d, x, ...) m(d, c-513, x) _ENUM_M512(c, m, d, __VA_ARGS__)
#define _ENUM_M514(c, m, d, x, ...) m(d, c-514, x) _ENUM_M513(c, m, d, __VA_ARGS__)
#define _ENUM_M515(c, m, d, x, ...) m(d, c-515, x) _ENUM_M514(c, m, d, __VA_ARGS__)
#define _ENUM_M516(c, m, d, x, ...) m(d, c-516, x) _ENUM_M515(c, m, d, __VA_ARGS__)
#define _ENUM_M517(c, m, d, x, ...) m(d, c-517, x) _ENUM_M516(c, m, d, __VA_ARGS__)
#define _ENUM_M518(c, m, d, x, ...) m(d, c-518, x) _ENUM_M517(c, m, d, __VA_ARGS__)
#define _ENUM_M519(c, m, d, x, ...) m(d, c-519, x) _ENUM_M518(c, m, d, __VA_ARGS__)
#define _ENUM_M520(c, m, d, x, ...) m(d, c-520, x) _ENUM_M519(c, m, d, __VA_ARGS__)
#define _ENUM_M521(c, m, d, x, ...) m(d, c-521, x) _ENUM_M520(c, m, d, __VA_ARGS__)
#define _ENUM_M522(c, m, d, x, ...) m(d, c-522, x) _ENUM_M521(c, m, d, __VA_ARGS__)
#define _ENUM_M523(c, m, d, x, ...) m(d, c-523, x) _ENUM_M522(c, m, d, __VA_ARGS__)
#define _ENUM_M524(c, m, d, x, ...) m(d, c-524, x) _ENUM_M523(c, m, d, __VA_ARGS__)
#define _ENUM_M525(c, m, d, x, ...) m(d, c-525, x) _ENUM_M524(c, m, d, __VA_ARGS__)
#define _ENUM_M526(c, m, d, x, ...) m(d, c-526, x) _ENUM_M525(c, m, d, __VA_ARGS__)
#define _ENUM_M527(c, m, d, x, ...) m(d, c-527, x) _ENUM_M526(c, m, d, __VA_ARGS__)
#define _ENUM_M528(c, m, d, x, ...) m(d, c-528, x) _ENUM_M527(c, m, d, __VA_ARGS__)
#define _ENUM_M529(c, m, d, x, ...) m(d, c-529, x) _ENUM_M528(c, m, d, __VA_ARGS__)
#define _ENUM_M530(c, m, d, x, ...) m(d, c-530, x) _ENUM_M529(c, m, d, __VA_ARGS__)
#define _ENUM_M531(c, m, d, x, ...) m(d, c-531, x) _ENUM_M530(c, m, d, __VA_ARGS__)
#define _ENUM_M532(c, m, d, x, ...) m(d, c-532, x) _ENUM_M531(c, m, d, __VA_ARGS__)
#define _ENUM_M533(c, m, d, x, ...) m(d, c-533, x) _ENUM_M532(c, m, d, __VA_ARGS__)
#define _ENUM_M534(c, m, d, x, ...) m(d, c-534, x) _ENUM_M533(c, m, d, __VA_ARGS__)
#define _ENUM_M535(c, m, d, x, ...) m(d, c-535, x) _ENUM_M534(c, m, d, __VA_ARGS__)
#define _ENUM_M536(c, m, d, x, ...) m(d, c-536, x) _ENUM_M535(c, m, d, __VA_ARGS__)
#define _ENUM_M537(c, m, d, x, ...) m(d, c-537, x) _ENUM_M536(c, m, d, __VA_ARGS__)
#define _ENUM_M538(c, m, d, x, ...) m(d, c-538, x) _ENUM_M537(c, m, d, __VA_ARGS__)
#define _ENUM_M539(c, m, d, x, ...) m(d, c-539, x) _ENUM_M538(c, m, d, __VA_ARGS__)
#define _ENUM_M540(c, m, d, x, ...) m(d, c-540, x) _ENUM_M539(c, m, d, __VA_ARGS__)
#define _ENUM_M541(c, m, d, x, ...) m(d, c-541, x) _ENUM_M540(c, m, d, __VA_ARGS__)
#define _ENUM_M542(c, m, d, x, ...) m(d, c-542, x) _ENUM_M541(c, m, d, __VA_ARGS__)
#define _ENUM_M543(c, m, d, x, ...) m(d, c-543, x) _ENUM_M542(c, m, d, __VA_ARGS__)
#define _ENUM_M544(c, m, d, x, ...) m(d, c-544, x) _ENUM_M543(c, m, d, __VA_ARGS__)
#define _ENUM_M545(c, m, d, x, ...) m(d, c-545, x) _ENUM_M544(c, m, d, __VA_ARGS__)
#define _ENUM_M546(c, m, d, x, ...) m(d, c-546, x) _ENUM_M545(c, m, d, __VA_ARGS__)
#define _ENUM_M547(c, m, d, x, ...) m(d, c-547, x) _ENUM_M546(c, m, d, __VA_ARGS__)
#define _ENUM_M548(c, m, d, x, ...) m(d, c-548, x) _ENUM_M547(c, m, d, __VA_ARGS__)
#define _ENUM_M549(c, m, d, x, ...) m(d, c-549, x) _ENUM_M548(c, m, d, __VA_ARGS__)
#define _ENUM_M550(c, m, d, x, ...) m(d, c-550, x) _ENUM_M549(c, m, d, __VA_ARGS__)
#define _ENUM_M551(c, m, d, x, ...) m(d, c-551, x) _ENUM_M550(c, m, d, __VA_ARGS__)
#define _ENUM_M552(c, m, d, x, ...) m(d, c-552, x) _ENUM_M551(c, m, d, __VA_ARGS__)
#define _ENUM_M553(c, m, d, x, ...) m(d, c-553, x) _ENUM_M552(c, m, d, __VA_ARGS__)
#define _ENUM_M554(c, m, d, x, ...) m(d, c-554, x) _ENUM_M553(c, m, d, __VA_ARGS__)
#define _ENUM_M555(c, m, d, x, ...) m(d, c-555, x) _ENUM_M554(c, m, d, __VA_ARGS__)
#define _ENUM_M556(c, m, d, x, ...) m(d, c-556, x) _ENUM_M555(c, m, d, __VA_ARGS__)
#define _ENUM_M557(c, m, d, x, ...) m(d, c-557, x) _ENUM_M556(c, m, d, __VA_ARGS__)
#define _ENUM_M558(c, m, d, x, ...) m(d, c-558, x) _ENUM_M557(c, m, d, __VA_ARGS__)
#define _ENUM_M559(c, m, d, x, ...) m(d, c-559, x) _ENUM_M558(c, m, d, __VA_ARGS__)
#define _ENUM_M560(c, m, d, x, ...) m(d, c-560, x) _ENUM_M559(c, m, d, __VA_ARGS__)
#define _ENUM_M561(c, m, d, x, ...) m(d, c-561, x) _ENUM_M560(c, m, d, __VA_ARGS__)
#define _ENUM_M562(c, m, d, x, ...) m(d, c-562, x) _ENUM_M561(c, m, d, __VA_ARGS__)
#define _ENUM_M563(c, m, d, x, ...) m(d, c-563, x) _ENUM_M562(c, m, d, __VA_ARGS__)
#define _ENUM_M564(c, m, d, x, ...) m(d, c-564, x) _ENUM_M563(c, m, d, __VA_ARGS__)
#define _ENUM_M565(c, m, d, x, ...) m(d, c-565, x) _ENUM_M564(c, m, d, __VA_ARGS__)
#define _ENUM_M566(c, m, d, x, ...) m(d, c-566, x) _ENUM_M565(c, m, d, __VA_ARGS__)
#define _ENUM_M567(c, m, d, x, ...) m(d, c-567, x) _ENUM_M566(c, m, d, __VA_ARGS__)
#define _ENUM_M568(c, m, d, x, ...) m(d, c-568, x) _ENUM_M567(c, m, d, __VA_ARGS__)
#define _ENUM_M569(c, m, d, x, ...) m(d, c-569, x) _ENUM_M568(c, m, d, __VA_ARGS__)
#define _ENUM_M570(c, m, d, x, ...) m(d, c-570, x) _ENUM_M569(c, m, d, __VA_ARGS__)
#define _ENUM_M571(c, m, d, x, ...) m(d, c-571, x) _ENUM_M570(c, m, d, __VA_ARGS__)
#define _ENUM_M572(c, m, d, x, ...) m(d, c-572, x) _ENUM_M571(c, m, d, __VA_ARGS__)
#define _ENUM_M573(c, m, d, x, ...) m(d, c-573, x) _ENUM_M572(c, m, d, __VA_ARGS__)
#define _ENUM_M574(c, m, d, x, ...) m(d, c-574, x) _ENUM_M573(c, m, d, __VA_ARGS__)
#define _ENUM_M575(c, m, d, x, ...) m(d, c-575, x) _ENUM_M574(c, m, d, __VA_ARGS__)
#define _ENUM_M576(c, m, d, x, ...) m(d, c-576, x) _ENUM_M575(c, m, d, __VA_ARGS__)
#define _ENUM_M577(c, m, d, x, ...) m(d, c-577, x) _ENUM_M576(c, m, d, __VA_ARGS__)
#define _ENUM_M578(c, m, d, x, ...) m(d, c-578, x) _ENUM_M577(c, m, d, __VA_ARGS__)
#define _ENUM_M579(c, m, d, x, ...) m(d, c-579, x) _ENUM_M578(c, m, d, __VA_ARGS__)
#define _ENUM_M580(c, m, d, x, ...) m(d, c-580, x) _ENUM_M579(c, m, d, __VA_ARGS__)
#define _ENUM_M581(c, m, d, x, ...) m(d, c-581, x) _ENUM_M580(c, m, d, __VA_ARGS__)
#define _ENUM_M582(c, m, d, x, ...) m(d, c-582, x) _ENUM_M581(c, m, d, __VA_ARGS__)
#define _ENUM_M583(c, m, d, x, ...) m(d, c-583, x) _ENUM_M582(c, m, d, __VA_ARGS__)
#define _ENUM_M584(c, m, d, x, ...) m(d, c-584, x) _ENUM_M583(c, m, d, __VA_ARGS__)
#define _ENUM_M585(c, m, d, x, ...) m(d, c-585, x) _ENUM_M584(c, m, d, __VA_ARGS__)
#define _ENUM_M586(c, m, d, x, ...) m(d, c-586, x) _ENUM_M585(c, m, d, __VA_ARGS__)
#define _ENUM_M587(c, m, d, x, ...) m(d, c-587, x) _ENUM_M586(c, m, d, __VA_ARGS__)
#define _ENUM_M588(c, m, d, x, ...) m(d, c-588, x) _ENUM_M587(c, m, d, __VA_ARGS__)
#define _ENUM_M589(c, m, d, x, ...) m(d, c-589, x) _ENUM_M588(c, m, d, __VA_ARGS__)
#define _ENUM_M590(c, m, d, x, ...) m(d, c-590, x) _ENUM_M589(c, m, d, __VA_ARGS__)
#define _ENUM_M591(c, m, d, x, ...) m(d, c-591, x) _ENUM_M590(c, m, d, __VA_ARGS__)
#define _ENUM_M592(c, m, d, x, ...) m(d, c-592, x) _ENUM_M591(c, m, d, __VA_ARGS__)
#define _ENUM_M593(c, m, d, x, ...) m(d, c-593, x) _ENUM_M592(c, m, d, __VA_ARGS__)
#define _ENUM_M594(c, m, d, x, ...) m(d, c-594, x) _ENUM_M593(c, m, d, __VA_ARGS__)
#define _ENUM_M595(c, m, d, x, ...) m(d, c-595, x) _ENUM_M594(c, m, d, __VA_ARGS__)
#define _ENUM_M596(c, m, d, x, ...) m(d, c-596, x) _ENUM_M595(c, m, d, __VA_ARGS__)
#define _ENUM_M597(c, m, d, x, ...) m(d, c-597, x) _ENUM_M596(c, m, d, __VA_ARGS__)
#define _ENUM_M598(c, m, d, x, ...) m(d, c-598, x) _ENUM_M597(c, m, d, __VA_ARGS__)
#define _ENUM_M599(c, m, d, x, ...) m(d, c-599, x) _ENUM_M598(c, m, d, __VA_ARGS__)
#define _ENUM_M600(c, m, d, x, ...) m(d, c-600, x) _ENUM_M599(c, m, d, __VA_ARGS__)
#define _ENUM_M601(c, m, d, x, ...) m(d, c-601, x) _ENUM_M600(c, m, d, __VA_ARGS__)
#define _ENUM_M602(c, m, d, x, ...) m(d, c-602, x) _ENUM_M601(c, m, d, __VA_ARGS__)
#define _ENUM_M603(c, m, d, x, ...) m(d, c-603, x) _ENUM_M602(c, m, d, __VA_ARGS__)
#define _ENUM_M604(c, m, d, x, ...) m(d, c-604, x) _ENUM_M603(c, m, d, __VA_ARGS__)
#define _ENUM_M605(c, m, d, x, ...) m(d, c-605, x) _ENUM_M604(c, m, d, __VA_ARGS__)
#define _ENUM_M606(c, m, d, x, ...) m(d, c-606, x) _ENUM_M605(c, m, d, __VA_ARGS__)
#define _ENUM_M607(c, m, d, x, ...) m(d, c-607, x) _ENUM_M606(c, m, d, __VA_ARGS__)
#define _ENUM_M608(c, m, d, x, ...) m(d, c-608, x) _ENUM_M607(c, m, d, __VA_ARGS__)
#define _ENUM_M609(c, m, d, x, ...) m(d, c-609, x) _ENUM_M608(c, m, d, __VA_ARGS__)
#define _ENUM_M610(c, m, d, x, ...) m(d, c-610, x) _ENUM_M609(c, m, d, __VA_ARGS__)
#define _ENUM_M611(c, m, d, x, ...) m(d, c-611, x) _ENUM_M610(c, m, d, __VA_ARGS__)
#define _ENUM_M612(c, m, d, x, ...) m(d, c-612, x) _ENUM_M611(c, m, d, __VA_ARGS__)
#define _ENUM_M613(c, m, d, x, ...) m(d, c-613, x) _ENUM_M612(c, m, d, __VA_ARGS__)
#define _ENUM_M614(c, m, d, x, ...) m(d, c-614, x) _ENUM_M613(c, m, d, __VA_ARGS__)
#define _ENUM_M615(c, m, d, x, ...) m(d, c-615, x) _ENUM_M614(c, m, d, __VA_ARGS__)
#define _ENUM_M616(c, m, d, x, ...) m(d, c-616, x) _ENUM_M615(c, m, d, __VA_ARGS__)
#define _ENUM_M617(c, m, d, x, ...) m(d, c-617, x) _ENUM_M616(c, m, d, __VA_ARGS__)
#define _ENUM_M618(c, m, d, x, ...) m(d, c-618, x) _ENUM_M617(c, m, d, __VA_ARGS__)
#define _ENUM_M619(c, m, d, x, ...) m(d, c-619, x) _ENUM_M618(c, m, d, __VA_ARGS__)
#define _ENUM_M620(c, m, d, x, ...) m(d, c-620, x) _ENUM_M619(c, m, d, __VA_ARGS__)
#define _ENUM_M621(c, m, d, x, ...) m(d, c-621, x) _ENUM_M620(c, m, d, __VA_ARGS__)
#define _ENUM_M622(c, m, d, x, ...) m(d, c-622, x) _ENUM_M621(c, m, d, __VA_ARGS__)
#define _ENUM_M623(c, m, d, x, ...) m(d, c-623, x) _ENUM_M622(c, m, d, __VA_ARGS__)
#define _ENUM_M624(c, m, d, x, ...) m(d, c-624, x) _ENUM_M623(c, m, d, __VA_ARGS__)
#define _ENUM_M625(c, m, d, x, ...) m(d, c-625, x) _ENUM_M624(c, m, d, __VA_ARGS__)
#define _ENUM_M626(c, m, d, x, ...) m(d, c-626, x) _ENUM_M625(c, m, d, __VA_ARGS__)
#define _ENUM_M627(c, m, d, x, ...) m(d, c-627, x) _ENUM_M626(c, m, d, __VA_ARGS__)
#define _ENUM_M628(c, m, d, x, ...) m(d, c-628, x) _ENUM_M627(c, m, d, __VA_ARGS__)
#define _ENUM_M629(c, m, d, x, ...) m(d, c-629, x) _ENUM_M628(c, m, d, __VA_ARGS__)
#define _ENUM_M630(c, m, d, x, ...) m(d, c-630, x) _ENUM_M629(c, m, d, __VA_ARGS__)
#define _ENUM_M631(c, m, d, x, ...) m(d, c-631, x) _ENUM_M630(c, m, d, __VA_ARGS__)
#define _ENUM_M632(c, m, d, x, ...) m(d, c-632, x) _ENUM_M631(c, m, d, __VA_ARGS__)
#define _ENUM_M633(c, m, d, x, ...) m(d, c-633, x) _ENUM_M632(c, m, d, __VA_ARGS__)
#define _ENUM_M634(c, m, d, x, ...) m(d, c-634, x) _ENUM_M633(c, m, d, __VA_ARGS__)
#define _ENUM_M635(c, m, d, x, ...) m(d, c-635, x) _ENUM_M634(c, m, d, __VA_ARGS__)
#define _ENUM_M636(c, m, d, x, ...) m(d, c-636, x) _ENUM_M635(c, m, d, __VA_ARGS__)
#define _ENUM_M637(c, m, d, x, ...) m(d, c-637, x) _ENUM_M636(c, m, d, __VA_ARGS__)
#define _ENUM_M638(c, m, d, x, ...) m(d, c-638, x) _ENUM_M637(c, m, d, __VA_ARGS__)
#define _ENUM_M639(c, m, d, x, ...) m(d, c-639, x) _ENUM_M638(c, m, d, __VA_ARGS__)
#define _ENUM_M640(c, m, d, x, ...) m(d, c-640, x) _ENUM_M639(c, m, d, __VA_ARGS__)
#define _ENUM_M641(c, m, d, x, ...) m(d, c-641, x) _ENUM_M640(c, m, d, __VA_ARGS__)
#define _ENUM_M642(c, m, d, x, ...) m(d, c-642, x) _ENUM_M641(c, m, d, __VA_ARGS__)
#define _ENUM_M643(c, m, d, x, ...) m(d, c-643, x) _ENUM_M642(c, m, d, __VA_ARGS__)
#define _ENUM_M644(c, m, d, x, ...) m(d, c-644, x) _ENUM_M643(c, m, d, __VA_ARGS__)
#define _ENUM_M645(c, m, d, x, ...) m(d, c-645, x) _ENUM_M644(c, m, d, __VA_ARGS__)
#define _ENUM_M646(c, m, d, x, ...) m(d, c-646, x) _ENUM_M645(c, m, d, __VA_ARGS__)
#define _ENUM_M647(c, m, d, x, ...) m(d, c-647, x) _ENUM_M646(c, m, d, __VA_ARGS__)
#define _ENUM_M648(c, m, d, x, ...) m(d, c-648, x) _ENUM_M647(c, m, d, __VA_ARGS__)
#define _ENUM_M649(c, m, d, x, ...) m(d, c-649, x) _ENUM_M648(c, m, d, __VA_ARGS__)
#define _ENUM_M650(c, m, d, x, ...) m(d, c-650, x) _ENUM_M649(c, m, d, __VA_ARGS__)
#define _ENUM_M651(c, m, d, x, ...) m(d, c-651, x) _ENUM_M650(c, m, d, __VA_ARGS__)
#define _ENUM_M652(c, m, d, x, ...) m(d, c-652, x) _ENUM_M651(c, m, d, __VA_ARGS__)
#define _ENUM_M653(c, m, d, x, ...) m(d, c-653, x) _ENUM_M652(c, m, d, __VA_ARGS__)
#define _ENUM_M654(c, m, d, x, ...) m(d, c-654, x) _ENUM_M653(c, m, d, __VA_ARGS__)
#define _ENUM_M655(c, m, d, x, ...) m(d, c-655, x) _ENUM_M654(c, m, d, __VA_ARGS__)
#define _ENUM_M656(c, m, d, x, ...) m(d, c-656, x) _ENUM_M655(c, m, d, __VA_ARGS__)
#define _ENUM_M657(c, m, d, x, ...) m(d, c-657, x) _ENUM_M656(c, m, d, __VA_ARGS__)
#define _ENUM_M658(c, m, d, x, ...) m(d, c-658, x) _ENUM_M657(c, m, d, __VA_ARGS__)
#define _ENUM_M659(c, m, d, x, ...) m(d, c-659, x) _ENUM_M658(c, m, d, __VA_ARGS__)
#define _ENUM_M660(c, m, d, x, ...) m(d, c-660, x) _ENUM_M659(c, m, d, __VA_ARGS__)
#define _ENUM_M661(c, m, d, x, ...) m(d, c-661, x) _ENUM_M660(c, m, d, __VA_ARGS__)
#define _ENUM_M662(c, m, d, x, ...) m(d, c-662, x) _ENUM_M661(c, m, d, __VA_ARGS__)
#define _ENUM_M663(c, m, d, x, ...) m(d, c-663, x) _ENUM_M662(c, m, d, __VA_ARGS__)
#define _ENUM_M664(c, m, d, x, ...) m(d, c-664, x) _ENUM_M663(c, m, d, __VA_ARGS__)
#define _ENUM_M665(c, m, d, x, ...) m(d, c-665, x) _ENUM_M664(c, m, d, __VA_ARGS__)
#define _ENUM_M666(c, m, d, x, ...) m(d, c-666, x) _ENUM_M665(c, m, d, __VA_ARGS__)
#define _ENUM_M667(c, m, d, x, ...) m(d, c-667, x) _ENUM_M666(c, m, d, __VA_ARGS__)
#define _ENUM_M668(c, m, d, x, ...) m(d, c-668, x) _ENUM_M667(c, m, d, __VA_ARGS__)
#define _ENUM_M669(c, m, d, x, ...) m(d, c-669, x) _ENUM_M668(c, m, d, __VA_ARGS__)
#define _ENUM_M670(c, m, d, x, ...) m(d, c-670, x) _ENUM_M669(c, m, d, __VA_ARGS__)
#define _ENUM_M671(c, m, d, x, ...) m(d, c-671, x) _ENUM_M670(c, m, d, __VA_ARGS__)
#define _ENUM_M672(c, m, d, x, ...) m(d, c-672, x) _ENUM_M671(c, m, d, __VA_ARGS__)
#define _ENUM_M673(c, m, d, x, ...) m(d, c-673, x) _ENUM_M672(c, m, d, __VA_ARGS__)
#define _ENUM_M674(c, m, d, x, ...) m(d, c-674, x) _ENUM_M673(c, m, d, __VA_ARGS__)
#define _ENUM_M675(c, m, d, x, ...) m(d, c-675, x) _ENUM_M674(c, m, d, __VA_ARGS__)
#define _ENUM_M676(c, m, d, x, ...) m(d, c-676, x) _ENUM_M675(c, m, d, __VA_ARGS__)
#define _ENUM_M677(c, m, d, x, ...) m(d, c-677, x) _ENUM_M676(c, m, d, __VA_ARGS__)
#define _ENUM_M678(c, m, d, x, ...) m(d, c-678, x) _ENUM_M677(c, m, d, __VA_ARGS__)
#define _ENUM_M679(c, m, d, x, ...) m(d, c-679, x) _ENUM_M678(c, m, d, __VA_ARGS__)
#define _ENUM_M680(c, m, d, x, ...) m(d, c-680, x) _ENUM_M679(c, m, d, __VA_ARGS__)
#define _ENUM_M681(c, m, d, x, ...) m(d, c-681, x) _ENUM_M680(c, m, d, __VA_ARGS__)
#define _ENUM_M682(c, m, d, x, ...) m(d, c-682, x) _ENUM_M681(c, m, d, __VA_ARGS__)
#define _ENUM_M683(c, m, d, x, ...) m(d, c-683, x) _ENUM_M682(c, m, d, __VA_ARGS__)
#define _ENUM_M684(c, m, d, x, ...) m(d, c-684, x) _ENUM_M683(c, m, d, __VA_ARGS__)
#define _ENUM_M685(c, m, d, x, ...) m(d, c-685, x) _ENUM_M684(c, m, d, __VA_ARGS__)
#define _ENUM_M686(c, m, d, x, ...) m(d, c-686, x) _ENUM_M685(c, m, d, __VA_ARGS__)
#define _ENUM_M687(c, m, d, x, ...) m(d, c-687, x) _ENUM_M686(c, m, d, __VA_ARGS__)
#define _ENUM_M688(c, m, d, x, ...) m(d, c-688, x) _ENUM_M687(c, m, d, __VA_ARGS__)
#define _ENUM_M689(c, m, d, x, ...) m(d, c-689, x) _ENUM_M688(c, m, d, __VA_ARGS__)
#define _ENUM_M690(c, m, d, x, ...) m(d, c-690, x) _ENUM_M689(c, m, d, __VA_ARGS__)
#define _ENUM_M691(c, m, d, x, ...) m(d, c-691, x) _ENUM_M690(c, m, d, __VA_ARGS__)
#define _ENUM_M692(c, m, d, x, ...) m(d, c-692, x) _ENUM_M691(c, m, d, __VA_ARGS__)
#define _ENUM_M693(c, m, d, x, ...) m(d, c-693, x) _ENUM_M692(c, m, d, __VA_ARGS__)
#define _ENUM_M694(c, m, d, x, ...) m(d, c-694, x) _ENUM_M693(c, m, d, __VA_ARGS__)
#define _ENUM_M695(c, m, d, x, ...) m(d, c-695, x) _ENUM_M694(c, m, d, __VA_ARGS__)
#define _ENUM_M696(c, m, d, x, ...) m(d, c-696, x) _ENUM_M695(c, m, d, __VA_ARGS__)
#define _ENUM_M697(c, m, d, x, ...) m(d, c-697, x) _ENUM_M696(c, m, d, __VA_ARGS__)
#define _ENUM_M698(c, m, d, x, ...) m(d, c-698, x) _ENUM_M697(c, m, d, __VA_ARGS__)
#define _ENUM_M699(c, m, d, x, ...) m(d, c-699, x) _ENUM_M698(c, m, d, __VA_ARGS__)
#define _ENUM_M700(c, m, d, x, ...) m(d, c-700, x) _ENUM_M699(c, m, d, __VA_ARGS__)
#define _ENUM_M701(c, m, d, x, ...) m(d, c-701, x) _ENUM_M700(c, m, d, __VA_ARGS__)
#define _ENUM_M702(c, m, d, x, ...) m(d, c-702, x) _ENUM_M701(c, m, d, __VA_ARGS__)
#define _ENUM_M703(c, m, d, x, ...) m(d, c-703, x) _ENUM_M702(c, m, d, __VA_ARGS__)
#define _ENUM_M704(c, m, d, x, ...) m(d, c-704, x) _ENUM_M703(c, m, d, __VA_ARGS__)
#define _ENUM_M705(c, m, d, x, ...) m(d, c-705, x) _ENUM_M704(c, m, d, __VA_ARGS__)
#define _ENUM_M706(c, m, d, x, ...) m(d, c-706, x) _ENUM_M705(c, m, d, __VA_ARGS__)
#define _ENUM_M707(c, m, d, x, ...) m(d, c-707, x) _ENUM_M706(c, m, d, __VA_ARGS__)
#define _ENUM_M708(c, m, d, x, ...) m(d, c-708, x) _ENUM_M707(c, m, d, __VA_ARGS__)
#define _ENUM_M709(c, m, d, x, ...) m(d, c-709, x) _ENUM_M708(c, m, d, __VA_ARGS__)
#define _ENUM_M710(c, m, d, x, ...) m(d, c-710, x) _ENUM_M709(c, m, d, __VA_ARGS__)
#define _ENUM_M711(c, m, d, x, ...) m(d, c-711, x) _ENUM_M710(c, m, d, __VA_ARGS__)
#define _ENUM_M712(c, m, d, x, ...) m(d, c-712, x) _ENUM_M711(c, m, d, __VA_ARGS__)
#define _ENUM_M713(c, m, d, x, ...) m(d, c-713, x) _ENUM_M712(c, m, d, __VA_ARGS__)
#define _ENUM_M714(c, m, d, x, ...) m(d, c-714, x) _ENUM_M713(c, m, d, __VA_ARGS__)
#define _ENUM_M715(c, m, d, x, ...) m(d, c-715, x) _ENUM_M714(c, m, d, __VA_ARGS__)
#define _ENUM_M716(c, m, d, x, ...) m(d, c-716, x) _ENUM_M715(c, m, d, __VA_ARGS__)
#define _ENUM_M717(c, m, d, x, ...) m(d, c-717, x) _ENUM_M716(c, m, d, __VA_ARGS__)
#define _ENUM_M718(c, m, d, x, ...) m(d, c-718, x) _ENUM_M717(c, m, d, __VA_ARGS__)
#define _ENUM_M719(c, m, d, x, ...) m(d, c-719, x) _ENUM_M718(c, m, d, __VA_ARGS__)
#define _ENUM_M720(c, m, d, x, ...) m(d, c-720, x) _ENUM_M719(c, m, d, __VA_ARGS__)
#define _ENUM_M721(c, m, d, x, ...) m(d, c-721, x) _ENUM_M720(c, m, d, __VA_ARGS__)
#define _ENUM_M722(c, m, d, x, ...) m(d, c-722, x) _ENUM_M721(c, m, d, __VA_ARGS__)
#define _ENUM_M723(c, m, d, x, ...) m(d, c-723, x) _ENUM_M722(c, m, d, __VA_ARGS__)
#define _ENUM_M724(c, m, d, x, ...) m(d, c-724, x) _ENUM_M723(c, m, d, __VA_ARGS__)
#define _ENUM_M725(c, m, d, x, ...) m(d, c-725, x) _ENUM_M724(c, m, d, __VA_ARGS__)
#define _ENUM_M726(c, m, d, x, ...) m(d, c-726, x) _ENUM_M725(c, m, d, __VA_ARGS__)
#define _ENUM_M727(c, m, d, x, ...) m(d, c-727, x) _ENUM_M726(c, m, d, __VA_ARGS__)
#define _ENUM_M728(c, m, d, x, ...) m(d, c-728, x) _ENUM_M727(c, m, d, __VA_ARGS__)
#define _ENUM_M729(c, m, d, x, ...) m(d, c-729, x) _ENUM_M728(c, m, d, __VA_ARGS__)
#define _ENUM_M730(c, m, d, x, ...) m(d, c-730, x) _ENUM_M729(c, m, d, __VA_ARGS__)
#define _ENUM_M731(c, m, d, x, ...) m(d, c-731, x) _ENUM_M730(c, m, d, __VA_ARGS__)
#define _ENUM_M732(c, m, d, x, ...) m(d, c-732, x) _ENUM_M731(c, m, d, __VA_ARGS__)
#define _ENUM_M733(c, m, d, x, ...) m(d, c-733, x) _ENUM_M732(c, m, d, __VA_ARGS__)
#define _ENUM_M734(c, m, d, x, ...) m(d, c-734, x) _ENUM_M733(c, m, d, __VA_ARGS__)
#define _ENUM_M735(c, m, d, x, ...) m(d, c-735, x) _ENUM_M734(c, m, d, __VA_ARGS__)
#define _ENUM_M736(c, m, d, x, ...) m(d, c-736, x) _ENUM_M735(c, m, d, __VA_ARGS__)
#define _ENUM_M737(c, m, d, x, ...) m(d, c-737, x) _ENUM_M736(c, m, d, __VA_ARGS__)
#define _ENUM_M738(c, m, d, x, ...) m(d, c-738, x) _ENUM_M737(c, m, d, __VA_ARGS__)
#define _ENUM_M739(c, m, d, x, ...) m(d, c-739, x) _ENUM_M738(c, m, d, __VA_ARGS__)
#define _ENUM_M740(c, m, d, x, ...) m(d, c-740, x) _ENUM_M739(c, m, d, __VA_ARGS__)
#define _ENUM_M741(c, m, d, x, ...) m(d, c-741, x) _ENUM_M740(c, m, d, __VA_ARGS__)
#define _ENUM_M742(c, m, d, x, ...) m(d, c-742, x) _ENUM_M741(c, m, d, __VA_ARGS__)
#define _ENUM_M743(c, m, d, x, ...) m(d, c-743, x) _ENUM_M742(c, m, d, __VA_ARGS__)
#define _ENUM_M744(c, m, d, x, ...) m(d, c-744, x) _ENUM_M743(c, m, d, __VA_ARGS__)
#define _ENUM_M745(c, m, d, x, ...) m(d, c-745, x) _ENUM_M744(c, m, d, __VA_ARGS__)
#define _ENUM_M746(c, m, d, x, ...) m(d, c-746, x) _ENUM_M745(c, m, d, __VA_ARGS__)
#define _ENUM_M747(c, m, d, x, ...) m(d, c-747, x) _ENUM_M746(c, m, d, __VA_ARGS__)
#define _ENUM_M748(c, m, d, x, ...) m(d, c-748, x) _ENUM_M747(c, m, d, __VA_ARGS__)
#define _ENUM_M749(c, m, d, x, ...) m(d, c-749, x) _ENUM_M748(c, m, d, __VA_ARGS__)
#define _ENUM_M750(c, m, d, x, ...) m(d, c-750, x) _ENUM_M749(c, m, d, __VA_ARGS__)
#define _ENUM_M751(c, m, d, x, ...) m(d, c-751, x) _ENUM_M750(c, m, d, __VA_ARGS__)
#define _ENUM_M752(c, m, d, x, ...) m(d, c-752, x) _ENUM_M751(c, m, d, __VA_ARGS__)
#define _ENUM_M753(c, m, d, x, ...) m(d, c-753, x) _ENUM_M752(c, m, d, __VA_ARGS__)
#define _ENUM_M754(c, m, d, x, ...) m(d, c-754, x) _ENUM_M753(c, m, d, __VA_ARGS__)
#define _ENUM_M755(c, m, d, x, ...) m(d, c-755, x) _ENUM_M754(c, m, d, __VA_ARGS__)
#define _ENUM_M756(c, m, d, x, ...) m(d, c-756, x) _ENUM_M755(c, m, d, __VA_ARGS__)
#define _ENUM_M757(c, m, d, x, ...) m(d, c-757, x) _ENUM_M756(c, m, d, __VA_ARGS__)
#define _ENUM_M758(c, m, d, x, ...) m(d, c-758, x) _ENUM_M757(c, m, d, __VA_ARGS__)
#define _ENUM_M759(c, m, d, x, ...) m(d, c-759, x) _ENUM_M758(c, m, d, __VA_ARGS__)
#define _ENUM_M760(c, m, d, x, ...) m(d, c-760, x) _ENUM_M759(c, m, d, __VA_ARGS__)
#define _ENUM_M761(c, m, d, x, ...) m(d, c-761, x) _ENUM_M760(c, m, d, __VA_ARGS__)
#define _ENUM_M762(c, m, d, x, ...) m(d, c-762, x) _ENUM_M761(c, m, d, __VA_ARGS__)
#define _ENUM_M763(c, m, d, x, ...) m(d, c-763, x) _ENUM_M762(c, m, d, __VA_ARGS__)
#define _ENUM_M764(c, m, d, x, ...) m(d, c-764, x) _ENUM_M763(c, m, d, __VA_ARGS__)
#define _ENUM_M765(c, m, d, x, ...) m(d, c-765, x) _ENUM_M764(c, m, d, __VA_ARGS__)
#define _ENUM_M766(c, m, d, x, ...) m(d, c-766, x) _ENUM_M765(c, m, d, __VA_ARGS__)
#define _ENUM_M767(c, m, d, x, ...) m(d, c-767, x) _ENUM_M766(c, m, d, __VA_ARGS__)
#define _ENUM_M768(c, m, d, x, ...) m(d, c-768, x) _ENUM_M767(c, m, d, __VA_ARGS__)
#define _ENUM_M769(c, m, d, x, ...) m(d, c-769, x) _ENUM_M768(c, m, d, __VA_ARGS__)
#define _ENUM_M770(c, m, d, x, ...) m(d, c-770, x) _ENUM_M769(c, m, d, __VA_ARGS__)
#define _ENUM_M771(c, m, d, x, ...) m(d, c-771, x) _ENUM_M770(c, m, d, __VA_ARGS__)
#define _ENUM_M772(c, m, d, x, ...) m(d, c-772, x) _ENUM_M771(c, m, d, __VA_ARGS__)
#define _ENUM_M773(c, m, d, x, ...) m(d, c-773, x) _ENUM_M772(c, m, d, __VA_ARGS__)
#define _ENUM_M774(c, m, d, x, ...) m(d, c-774, x) _ENUM_M773(c, m, d, __VA_ARGS__)
#define _ENUM_M775(c, m, d, x, ...) m(d, c-775, x) _ENUM_M774(c, m, d, __VA_ARGS__)
#define _ENUM_M776(c, m, d, x, ...) m(d, c-776, x) _ENUM_M775(c, m, d, __VA_ARGS__)
#define _ENUM_M777(c, m, d, x, ...) m(d, c-777, x) _ENUM_M776(c, m, d, __VA_ARGS__)
#define _ENUM_M778(c, m, d, x, ...) m(d, c-778, x) _ENUM_M777(c, m, d, __VA_ARGS__)
#define _ENUM_M779(c, m, d, x, ...) m(d, c-779, x) _ENUM_M778(c, m, d, __VA_ARGS__)
#define _ENUM_M780(c, m, d, x, ...) m(d, c-780, x) _ENUM_M779(c, m, d, __VA_ARGS__)
#define _ENUM_M781(c, m, d, x, ...) m(d, c-781, x) _ENUM_M780(c, m, d, __VA_ARGS__)
#define _ENUM_M782(c, m, d, x, ...) m(d, c-782, x) _ENUM_M781(c, m, d, __VA_ARGS__)
#define _ENUM_M783(c, m, d, x, ...) m(d, c-783, x) _ENUM_M782(c, m, d, __VA_ARGS__)
#define _ENUM_M784(c, m, d, x, ...) m(d, c-784, x) _ENUM_M783(c, m, d, __VA_ARGS__)
#define _ENUM_M785(c, m, d, x, ...) m(d, c-785, x) _ENUM_M784(c, m, d, __VA_ARGS__)
#define _ENUM_M786(c, m, d, x, ...) m(d, c-786, x) _ENUM_M785(c, m, d, __VA_ARGS__)
#define _ENUM_M787(c, m, d, x, ...) m(d, c-787, x) _ENUM_M786(c, m, d, __VA_ARGS__)
#define _ENUM_M788(c, m, d, x, ...) m(d, c-788, x) _ENUM_M787(c, m, d, __VA_ARGS__)
#define _ENUM_M789(c, m, d, x, ...) m(d, c-789, x) _ENUM_M788(c, m, d, __VA_ARGS__)
#define _ENUM_M790(c, m, d, x, ...) m(d, c-790, x) _ENUM_M789(c, m, d, __VA_ARGS__)
#define _ENUM_M791(c, m, d, x, ...) m(d, c-791, x) _ENUM_M790(c, m, d, __VA_ARGS__)
#define _ENUM_M792(c, m, d, x, ...) m(d, c-792, x) _ENUM_M791(c, m, d, __VA_ARGS__)
#define _ENUM_M793(c, m, d, x, ...) m(d, c-793, x) _ENUM_M792(c, m, d, __VA_ARGS__)
#define _ENUM_M794(c, m, d, x, ...) m(d, c-794, x) _ENUM_M793(c, m, d, __VA_ARGS__)
#define _ENUM_M795(c, m, d, x, ...) m(d, c-795, x) _ENUM_M794(c, m, d, __VA_ARGS__)
#define _ENUM_M796(c, m, d, x, ...) m(d, c-796, x) _ENUM_M795(c, m, d, __VA_ARGS__)
#define _ENUM_M797(c, m, d, x, ...) m(d, c-797, x) _ENUM_M796(c, m, d, __VA_ARGS__)
#define _ENUM_M798(c, m, d, x, ...) m(d, c-798, x) _ENUM_M797(c, m, d, __VA_ARGS__)
#define _ENUM_M799(c, m, d, x, ...) m(d, c-799, x) _ENUM_M798(c, m, d, __VA_ARGS__)
#define _ENUM_M800(c, m, d, x, ...) m(d, c-800, x) _ENUM_M799(c, m, d, __VA_ARGS__)
#define _ENUM_M801(c, m, d, x, ...) m(d, c-801, x) _ENUM_M800(c, m, d, __VA_ARGS__)
#define _ENUM_M802(c, m, d, x, ...) m(d, c-802, x) _ENUM_M801(c, m, d, __VA_ARGS__)
#define _ENUM_M803(c, m, d, x, ...) m(d, c-803, x) _ENUM_M802(c, m, d, __VA_ARGS__)
#define _ENUM_M804(c, m, d, x, ...) m(d, c-804, x) _ENUM_M803(c, m, d, __VA_ARGS__)
#define _ENUM_M805(c, m, d, x, ...) m(d, c-805, x) _ENUM_M804(c, m, d, __VA_ARGS__)
#define _ENUM_M806(c, m, d, x, ...) m(d, c-806, x) _ENUM_M805(c, m, d, __VA_ARGS__)
#define _ENUM_M807(c, m, d, x, ...) m(d, c-807, x) _ENUM_M806(c, m, d, __VA_ARGS__)
#define _ENUM_M808(c, m, d, x, ...) m(d, c-808, x) _ENUM_M807(c, m, d, __VA_ARGS__)
#define _ENUM_M809(c, m, d, x, ...) m(d, c-809, x) _ENUM_M808(c, m, d, __VA_ARGS__)
#define _ENUM_M810(c, m, d, x, ...) m(d, c-810, x) _ENUM_M809(c, m, d, __VA_ARGS__)
#define _ENUM_M811(c, m, d, x, ...) m(d, c-811, x) _ENUM_M810(c, m, d, __VA_ARGS__)
#define _ENUM_M812(c, m, d, x, ...) m(d, c-812, x) _ENUM_M811(c, m, d, __VA_ARGS__)
#define _ENUM_M813(c, m, d, x, ...) m(d, c-813, x) _ENUM_M812(c, m, d, __VA_ARGS__)
#define _ENUM_M814(c, m, d, x, ...) m(d, c-814, x) _ENUM_M813(c, m, d, __VA_ARGS__)
#define _ENUM_M815(c, m, d, x, ...) m(d, c-815, x) _ENUM_M814(c, m, d, __VA_ARGS__)
#define _ENUM_M816(c, m, d, x, ...) m(d, c-816, x) _ENUM_M815(c, m, d, __VA_ARGS__)
#define _ENUM_M817(c, m, d, x, ...) m(d, c-817, x) _ENUM_M816(c, m, d, __VA_ARGS__)
#define _ENUM_M818(c, m, d, x, ...) m(d, c-818, x) _ENUM_M817(c, m, d, __VA_ARGS__)
#define _ENUM_M819(c, m, d, x, ...) m(d, c-819, x) _ENUM_M818(c, m, d, __VA_ARGS__)
#define _ENUM_M820(c, m, d, x, ...) m(d, c-820, x) _ENUM_M819(c, m, d, __VA_ARGS__)
#define _ENUM_M821(c, m, d, x, ...) m(d, c-821, x) _ENUM_M820(c, m, d, __VA_ARGS__)
#define _ENUM_M822(c, m, d, x, ...) m(d, c-822, x) _ENUM_M821(c, m, d, __VA_ARGS__)
#define _ENUM_M823(c, m, d, x, ...) m(d, c-823, x) _ENUM_M822(c, m, d, __VA_ARGS__)
#define _ENUM_M824(c, m, d, x, ...) m(d, c-824, x) _ENUM_M823(c, m, d, __VA_ARGS__)
#define _ENUM_M825(c, m, d, x, ...) m(d, c-825, x) _ENUM_M824(c, m, d, __VA_ARGS__)
#define _ENUM_M826(c, m, d, x, ...) m(d, c-826, x) _ENUM_M825(c, m, d, __VA_ARGS__)
#define _ENUM_M827(c, m, d, x, ...) m(d, c-827, x) _ENUM_M826(c, m, d, __VA_ARGS__)
#define _ENUM_M828(c, m, d, x, ...) m(d, c-828, x) _ENUM_M827(c, m, d, __VA_ARGS__)
#define _ENUM_M829(c, m, d, x, ...) m(d, c-829, x) _ENUM_M828(c, m, d, __VA_ARGS__)
#define _ENUM_M830(c, m, d, x, ...) m(d, c-830, x) _ENUM_M829(c, m, d, __VA_ARGS__)
#define _ENUM_M831(c, m, d, x, ...) m(d, c-831, x) _ENUM_M830(c, m, d, __VA_ARGS__)
#define _ENUM_M832(c, m, d, x, ...) m(d, c-832, x) _ENUM_M831(c, m, d, __VA_ARGS__)
#define _ENUM_M833(c, m, d, x, ...) m(d, c-833, x) _ENUM_M832(c, m, d, __VA_ARGS__)
#define _ENUM_M834(c, m, d, x, ...) m(d, c-834, x) _ENUM_M833(c, m, d, __VA_ARGS__)
#define _ENUM_M835(c, m, d, x, ...) m(d, c-835, x) _ENUM_M834(c, m, d, __VA_ARGS__)
#define _ENUM_M836(c, m, d, x, ...) m(d, c-836, x) _ENUM_M835(c, m, d, __VA_ARGS__)
#define _ENUM_M837(c, m, d, x, ...) m(d, c-837, x) _ENUM_M836(c, m, d, __VA_ARGS__)
#define _ENUM_M838(c, m, d, x, ...) m(d, c-838, x) _ENUM_M837(c, m, d, __VA_ARGS__)
#define _ENUM_M839(c, m, d, x, ...) m(d, c-839, x) _ENUM_M838(c, m, d, __VA_ARGS__)
#define _ENUM_M840(c, m, d, x, ...) m(d, c-840, x) _ENUM_M839(c, m, d, __VA_ARGS__)
#define _ENUM_M841(c, m, d, x, ...) m(d, c-841, x) _ENUM_M840(c, m, d, __VA_ARGS__)
#define _ENUM_M842(c, m, d, x, ...) m(d, c-842, x) _ENUM_M841(c, m, d, __VA_ARGS__)
#define _ENUM_M843(c, m, d, x, ...) m(d, c-843, x) _ENUM_M842(c, m, d, __VA_ARGS__)
#define _ENUM_M844(c, m, d, x, ...) m(d, c-844, x) _ENUM_M843(c, m, d, __VA_ARGS__)
#define _ENUM_M845(c, m, d, x, ...) m(d, c-845, x) _ENUM_M844(c, m, d, __VA_ARGS__)
#define _ENUM_M846(c, m, d, x, ...) m(d, c-846, x) _ENUM_M845(c, m, d, __VA_ARGS__)
#define _ENUM_M847(c, m, d, x, ...) m(d, c-847, x) _ENUM_M846(c, m, d, __VA_ARGS__)
#define _ENUM_M848(c, m, d, x, ...) m(d, c-848, x) _ENUM_M847(c, m, d, __VA_ARGS__)
#define _ENUM_M849(c, m, d, x, ...) m(d, c-849, x) _ENUM_M848(c, m, d, __VA_ARGS__)
#define _ENUM_M850(c, m, d, x, ...) m(d, c-850, x) _ENUM_M849(c, m, d, __VA_ARGS__)
#define _ENUM_M851(c, m, d, x, ...) m(d, c-851, x) _ENUM_M850(c, m, d, __VA_ARGS__)
#define _ENUM_M852(c, m, d, x, ...) m(d, c-852, x) _ENUM_M851(c, m, d, __VA_ARGS__)
#define _ENUM_M853(c, m, d, x, ...) m(d, c-853, x) _ENUM_M852(c, m, d, __VA_ARGS__)
#define _ENUM_M854(c, m, d, x, ...) m(d, c-854, x) _ENUM_M853(c, m, d, __VA_ARGS__)
#define _ENUM_M855(c, m, d, x, ...) m(d, c-855, x) _ENUM_M854(c, m, d, __VA_ARGS__)
#define _ENUM_M856(c, m, d, x, ...) m(d, c-856, x) _ENUM_M855(c, m, d, __VA_ARGS__)
#define _ENUM_M857(c, m, d, x, ...) m(d, c-857, x) _ENUM_M856(c, m, d, __VA_ARGS__)
#define _ENUM_M858(c, m, d, x, ...) m(d, c-858, x) _ENUM_M857(c, m, d, __VA_ARGS__)
#define _ENUM_M859(c, m, d, x, ...) m(d, c-859, x) _ENUM_M858(c, m, d, __VA_ARGS__)
#define _ENUM_M860(c, m, d, x, ...) m(d, c-860, x) _ENUM_M859(c, m, d, __VA_ARGS__)
#define _ENUM_M861(c, m, d, x, ...) m(d, c-861, x) _ENUM_M860(c, m, d, __VA_ARGS__)
#define _ENUM_M862(c, m, d, x, ...) m(d, c-862, x) _ENUM_M861(c, m, d, __VA_ARGS__)
#define _ENUM_M863(c, m, d, x, ...) m(d, c-863, x) _ENUM_M862(c, m, d, __VA_ARGS__)
#define _ENUM_M864(c, m, d, x, ...) m(d, c-864, x) _ENUM_M863(c, m, d, __VA_ARGS__)
#define _ENUM_M865(c, m, d, x, ...) m(d, c-865, x) _ENUM_M864(c, m, d, __VA_ARGS__)
#define _ENUM_M866(c, m, d, x, ...) m(d, c-866, x) _ENUM_M865(c, m, d, __VA_ARGS__)
#define _ENUM_M867(c, m, d, x, ...) m(d, c-867, x) _ENUM_M866(c, m, d, __VA_ARGS__)
#define _ENUM_M868(c, m, d, x, ...) m(d, c-868, x) _ENUM_M867(c, m, d, __VA_ARGS__)
#define _ENUM_M869(c, m, d, x, ...) m(d, c-869, x) _ENUM_M868(c, m, d, __VA_ARGS__)
#define _ENUM_M870(c, m, d, x, ...) m(d, c-870, x) _ENUM_M869(c, m, d, __VA_ARGS__)
#define _ENUM_M871(c, m, d, x, ...) m(d, c-871, x) _ENUM_M870(c, m, d, __VA_ARGS__)
#define _ENUM_M872(c, m, d, x, ...) m(d, c-872, x) _ENUM_M871(c, m, d, __VA_ARGS__)
#define _ENUM_M873(c, m, d, x, ...) m(d, c-873, x) _ENUM_M872(c, m, d, __VA_ARGS__)
#define _ENUM_M874(c, m, d, x, ...) m(d, c-874, x) _ENUM_M873(c, m, d, __VA_ARGS__)
#define _ENUM_M875(c, m, d, x, ...) m(d, c-875, x) _ENUM_M874(c, m, d, __VA_ARGS__)
#define _ENUM_M876(c, m, d, x, ...) m(d, c-876, x) _ENUM_M875(c, m, d, __VA_ARGS__)
#define _ENUM_M877(c, m, d, x, ...) m(d, c-877, x) _ENUM_M876(c, m, d, __VA_ARGS__)
#define _ENUM_M878(c, m, d, x, ...) m(d, c-878, x) _ENUM_M877(c, m, d, __VA_ARGS__)
#define _ENUM_M879(c, m, d, x, ...) m(d, c-879, x) _ENUM_M878(c, m, d, __VA_ARGS__)
#define _ENUM_M880(c, m, d, x, ...) m(d, c-880, x) _ENUM_M879(c, m, d, __VA_ARGS__)
#define _ENUM_M881(c, m, d, x, ...) m(d, c-881, x) _ENUM_M880(c, m, d, __VA_ARGS__)
#define _ENUM_M882(c, m, d, x, ...) m(d, c-882, x) _ENUM_M881(c, m, d, __VA_ARGS__)
#define _ENUM_M883(c, m, d, x, ...) m(d, c-883, x) _ENUM_M882(c, m, d, __VA_ARGS__)
#define _ENUM_M884(c, m, d, x, ...) m(d, c-884, x) _ENUM_M883(c, m, d, __VA_ARGS__)
#define _ENUM_M885(c, m, d, x, ...) m(d, c-885, x) _ENUM_M884(c, m, d, __VA_ARGS__)
#define _ENUM_M886(c, m, d, x, ...) m(d, c-886, x) _ENUM_M885(c, m, d, __VA_ARGS__)
#define _ENUM_M887(c, m, d, x, ...) m(d, c-887, x) _ENUM_M886(c, m, d, __VA_ARGS__)
#define _ENUM_M888(c, m, d, x, ...) m(d, c-888, x) _ENUM_M887(c, m, d, __VA_ARGS__)
#define _ENUM_M889(c, m, d, x, ...) m(d, c-889, x) _ENUM_M888(c, m, d, __VA_ARGS__)
#define _ENUM_M890(c, m, d, x, ...) m(d, c-890, x) _ENUM_M889(c, m, d, __VA_ARGS__)
#define _ENUM_M891(c, m, d, x, ...) m(d, c-891, x) _ENUM_M890(c, m, d, __VA_ARGS__)
#define _ENUM_M892(c, m, d, x, ...) m(d, c-892, x) _ENUM_M891(c, m, d, __VA_ARGS__)
#define _ENUM_M893(c, m, d, x, ...) m(d, c-893, x) _ENUM_M892(c, m, d, __VA_ARGS__)
#define _ENUM_M894(c, m, d, x, ...) m(d, c-894, x) _ENUM_M893(c, m, d, __VA_ARGS__)
#define _ENUM_M895(c, m, d, x, ...) m(d, c-895, x) _ENUM_M894(c, m, d, __VA_ARGS__)
#define _ENUM_M896(c, m, d, x, ...) m(d, c-896, x) _ENUM_M895(c, m, d, __VA_ARGS__)
#define _ENUM_M897(c, m, d, x, ...) m(d, c-897, x) _ENUM_M896(c, m, d, __VA_ARGS__)
#define _ENUM_M898(c, m, d, x, ...) m(d, c-898, x) _ENUM_M897(c, m, d, __VA_ARGS__)
#define _ENUM_M899(c, m, d, x, ...) m(d, c-899, x) _ENUM_M898(c, m, d, __VA_ARGS__)
#define _ENUM_M900(c, m, d, x, ...) m(d, c-900, x) _ENUM_M899(c, m, d, __VA_ARGS__)
#define _ENUM_M901(c, m, d, x, ...) m(d, c-901, x) _ENUM_M900(c, m, d, __VA_ARGS__)
#define _ENUM_M902(c, m, d, x, ...) m(d, c-902, x) _ENUM_M901(c, m, d, __VA_ARGS__)
#define _ENUM_M903(c, m, d, x, ...) m(d, c-903, x) _ENUM_M902(c, m, d, __VA_ARGS__)
#define _ENUM_M904(c, m, d, x, ...) m(d, c-904, x) _ENUM_M903(c, m, d, __VA_ARGS__)
#define _ENUM_M905(c, m, d, x, ...) m(d, c-905, x) _ENUM_M904(c, m, d, __VA_ARGS__)
#define _ENUM_M906(c, m, d, x, ...) m(d, c-906, x) _ENUM_M905(c, m, d, __VA_ARGS__)
#define _ENUM_M907(c, m, d, x, ...) m(d, c-907, x) _ENUM_M906(c, m, d, __VA_ARGS__)
#define _ENUM_M908(c, m, d, x, ...) m(d, c-908, x) _ENUM_M907(c, m, d, __VA_ARGS__)
#define _ENUM_M909(c, m, d, x, ...) m(d, c-909, x) _ENUM_M908(c, m, d, __VA_ARGS__)
#define _ENUM_M910(c, m, d, x, ...) m(d, c-910, x) _ENUM_M909(c, m, d, __VA_ARGS__)
#define _ENUM_M911(c, m, d, x, ...) m(d, c-911, x) _ENUM_M910(c, m, d, __VA_ARGS__)
#define _ENUM_M912(c, m, d, x, ...) m(d, c-912, x) _ENUM_M911(c, m, d, __VA_ARGS__)
#define _ENUM_M913(c, m, d, x, ...) m(d, c-913, x) _ENUM_M912(c, m, d, __VA_ARGS__)
#define _ENUM_M914(c, m, d, x, ...) m(d, c-914, x) _ENUM_M913(c, m, d, __VA_ARGS__)
#define _ENUM_M915(c, m, d, x, ...) m(d, c-915, x) _ENUM_M914(c, m, d, __VA_ARGS__)
#define _ENUM_M916(c, m, d, x, ...) m(d, c-916, x) _ENUM_M915(c, m, d, __VA_ARGS__)
#define _ENUM_M917(c, m, d, x, ...) m(d, c-917, x) _ENUM_M916(c, m, d, __VA_ARGS__)
#define _ENUM_M918(c, m, d, x, ...) m(d, c-918, x) _ENUM_M917(c, m, d, __VA_ARGS__)
#define _ENUM_M919(c, m, d, x, ...) m(d, c-919, x) _ENUM_M918(c, m, d, __VA_ARGS__)
#define _ENUM_M920(c, m, d, x, ...) m(d, c-920, x) _ENUM_M919(c, m, d, __VA_ARGS__)
#define _ENUM_M921(c, m, d, x, ...) m(d, c-921, x) _ENUM_M920(c, m, d, __VA_ARGS__)
#define _ENUM_M922(c, m, d, x, ...) m(d, c-922, x) _ENUM_M921(c, m, d, __VA_ARGS__)
#define _ENUM_M923(c, m, d, x, ...) m(d, c-923, x) _ENUM_M922(c, m, d, __VA_ARGS__)
#define _ENUM_M924(c, m, d, x, ...) m(d, c-924, x) _ENUM_M923(c, m, d, __VA_ARGS__)
#define _ENUM_M925(c, m, d, x, ...) m(d, c-925, x) _ENUM_M924(c, m, d, __VA_ARGS__)
#define _ENUM_M926(c, m, d, x, ...) m(d, c-926, x) _ENUM_M925(c, m, d, __VA_ARGS__)
#define _ENUM_M927(c, m, d, x, ...) m(d, c-927, x) _ENUM_M926(c, m, d, __VA_ARGS__)
#define _ENUM_M928(c, m, d, x, ...) m(d, c-928, x) _ENUM_M927(c, m, d, __VA_ARGS__)
#define _ENUM_M929(c, m, d, x, ...) m(d, c-929, x) _ENUM_M928(c, m, d, __VA_ARGS__)
#define _ENUM_M930(c, m, d, x, ...) m(d, c-930, x) _ENUM_M929(c, m, d, __VA_ARGS__)
#define _ENUM_M931(c, m, d, x, ...) m(d, c-931, x) _ENUM_M930(c, m, d, __VA_ARGS__)
#define _ENUM_M932(c, m, d, x, ...) m(d, c-932, x) _ENUM_M931(c, m, d, __VA_ARGS__)
#define _ENUM_M933(c, m, d, x, ...) m(d, c-933, x) _ENUM_M932(c, m, d, __VA_ARGS__)
#define _ENUM_M934(c, m, d, x, ...) m(d, c-934, x) _ENUM_M933(c, m, d, __VA_ARGS__)
#define _ENUM_M935(c, m, d, x, ...) m(d, c-935, x) _ENUM_M934(c, m, d, __VA_ARGS__)
#define _ENUM_M936(c, m, d, x, ...) m(d, c-936, x) _ENUM_M935(c, m, d, __VA_ARGS__)
#define _ENUM_M937(c, m, d, x, ...) m(d, c-937, x) _ENUM_M936(c, m, d, __VA_ARGS__)
#define _ENUM_M938(c, m, d, x, ...) m(d, c-938, x) _ENUM_M937(c, m, d, __VA_ARGS__)
#define _ENUM_M939(c, m, d, x, ...) m(d, c-939, x) _ENUM_M938(c, m, d, __VA_ARGS__)
#define _ENUM_M940(c, m, d, x, ...) m(d, c-940, x) _ENUM_M939(c, m, d, __VA_ARGS__)
#define _ENUM_M941(c, m, d, x, ...) m(d, c-941, x) _ENUM_M940(c, m, d, __VA_ARGS__)
#define _ENUM_M942(c, m, d, x, ...) m(d, c-942, x) _ENUM_M941(c, m, d, __VA_ARGS__)
#define _ENUM_M943(c, m, d, x, ...) m(d, c-943, x) _ENUM_M942(c, m, d, __VA_ARGS__)
#define _ENUM_M944(c, m, d, x, ...) m(d, c-944, x) _ENUM_M943(c, m, d, __VA_ARGS__)
#define _ENUM_M945(c, m, d, x, ...) m(d, c-945, x) _ENUM_M944(c, m, d, __VA_ARGS__)
#define _ENUM_M946(c, m, d, x, ...) m(d, c-946, x) _ENUM_M945(c, m, d, __VA_ARGS__)
#define _ENUM_M947(c, m, d, x, ...) m(d, c-947, x) _ENUM_M946(c, m, d, __VA_ARGS__)
#define _ENUM_M948(c, m, d, x, ...) m(d, c-948, x) _ENUM_M947(c, m, d, __VA_ARGS__)
#define _ENUM_M949(c, m, d, x, ...) m(d, c-949, x) _ENUM_M948(c, m, d, __VA_ARGS__)
#define _ENUM_M950(c, m, d, x, ...) m(d, c-950, x) _ENUM_M949(c, m, d, __VA_ARGS__)
#define _ENUM_M951(c, m, d, x, ...) m(d, c-951, x) _ENUM_M950(c, m, d, __VA_ARGS__)
#define _ENUM_M952(c, m, d, x, ...) m(d, c-952, x) _ENUM_M951(c, m, d, __VA_ARGS__)
#define _ENUM_M953(c, m, d, x, ...) m(d, c-953, x) _ENUM_M952(c, m, d, __VA_ARGS__)
#define _ENUM_M954(c, m, d, x, ...) m(d, c-954, x) _ENUM_M953(c, m, d, __VA_ARGS__)
#define _ENUM_M955(c, m, d, x, ...) m(d, c-955, x) _ENUM_M954(c, m, d, __VA_ARGS__)
#define _ENUM_M956(c, m, d, x, ...) m(d, c-956, x) _ENUM_M955(c, m, d, __VA_ARGS__)
#define _ENUM_M957(c, m, d, x, ...) m(d, c-957, x) _ENUM_M956(c, m, d, __VA_ARGS__)
#define _ENUM_M958(c, m, d, x, ...) m(d, c-958, x) _ENUM_M957(c, m, d, __VA_ARGS__)
#define _ENUM_M959(c, m, d, x, ...) m(d, c-959, x) _ENUM_M958(c, m, d, __VA_ARGS__)
#define _ENUM_M960(c, m, d, x, ...) m(d, c-960, x) _ENUM_M959(c, m, d, __VA_ARGS__)
#define _ENUM_M961(c, m, d, x, ...) m(d, c-961, x) _ENUM_M960(c, m, d, __VA_ARGS__)
#define _ENUM_M962(c, m, d, x, ...) m(d, c-962, x) _ENUM_M961(c, m, d, __VA_ARGS__)
#define _ENUM_M963(c, m, d, x, ...) m(d, c-963, x) _ENUM_M962(c, m, d, __VA_ARGS__)
#define _ENUM_M964(c, m, d, x, ...) m(d, c-964, x) _ENUM_M963(c, m, d, __VA_ARGS__)
#define _ENUM_M965(c, m, d, x, ...) m(d, c-965, x) _ENUM_M964(c, m, d, __VA_ARGS__)
#define _ENUM_M966(c, m, d, x, ...) m(d, c-966, x) _ENUM_M965(c, m, d, __VA_ARGS__)
#define _ENUM_M967(c, m, d, x, ...) m(d, c-967, x) _ENUM_M966(c, m, d, __VA_ARGS__)
#define _ENUM_M968(c, m, d, x, ...) m(d, c-968, x) _ENUM_M967(c, m, d, __VA_ARGS__)
#define _ENUM_M969(c, m, d, x, ...) m(d, c-969, x) _ENUM_M968(c, m, d, __VA_ARGS__)
#define _ENUM_M970(c, m, d, x, ...) m(d, c-970, x) _ENUM_M969(c, m, d, __VA_ARGS__)
#define _ENUM_M971(c, m, d, x, ...) m(d, c-971, x) _ENUM_M970(c, m, d, __VA_ARGS__)
#define _ENUM_M972(c, m, d, x, ...) m(d, c-972, x) _ENUM_M971(c, m, d, __VA_ARGS__)
#define _ENUM_M973(c, m, d, x, ...) m(d, c-973, x) _ENUM_M972(c, m, d, __VA_ARGS__)
#define _ENUM_M974(c, m, d, x, ...) m(d, c-974, x) _ENUM_M973(c, m, d, __VA_ARGS__)
#define _ENUM_M975(c, m, d, x, ...) m(d, c-975, x) _ENUM_M974(c, m, d, __VA_ARGS__)
#define _ENUM_M976(c, m, d, x, ...) m(d, c-976, x) _ENUM_M975(c, m, d, __VA_ARGS__)
#define _ENUM_M977(c, m, d, x, ...) m(d, c-977, x) _ENUM_M976(c, m, d, __VA_ARGS__)
#define _ENUM_M978(c, m, d, x, ...) m(d, c-978, x) _ENUM_M977(c, m, d, __VA_ARGS__)
#define _ENUM_M979(c, m, d, x, ...) m(d, c-979, x) _ENUM_M978(c, m, d, __VA_ARGS__)
#define _ENUM_M980(c, m, d, x, ...) m(d, c-980, x) _ENUM_M979(c, m, d, __VA_ARGS__)
#define _ENUM_M981(c, m, d, x, ...) m(d, c-981, x) _ENUM_M980(c, m, d, __VA_ARGS__)
#define _ENUM_M982(c, m, d, x, ...) m(d, c-982, x) _ENUM_M981(c, m, d, __VA_ARGS__)
#define _ENUM_M983(c, m, d, x, ...) m(d, c-983, x) _ENUM_M982(c, m, d, __VA_ARGS__)
#define _ENUM_M984(c, m, d, x, ...) m(d, c-984, x) _ENUM_M983(c, m, d, __VA_ARGS__)
#define _ENUM_M985(c, m, d, x, ...) m(d, c-985, x) _ENUM_M984(c, m, d, __VA_ARGS__)
#define _ENUM_M986(c, m, d, x, ...) m(d, c-986, x) _ENUM_M985(c, m, d, __VA_ARGS__)
#define _ENUM_M987(c, m, d, x, ...) m(d, c-987, x) _ENUM_M986(c, m, d, __VA_ARGS__)
#define _ENUM_M988(c, m, d, x, ...) m(d, c-988, x) _ENUM_M987(c, m, d, __VA_ARGS__)
#define _ENUM_M989(c, m, d, x, ...) m(d, c-989, x) _ENUM_M988(c, m, d, __VA_ARGS__)
#define _ENUM_M990(c, m, d, x, ...) m(d, c-990, x) _ENUM_M989(c, m, d, __VA_ARGS__)
#define _ENUM_M991(c, m, d, x, ...) m(d, c-991, x) _ENUM_M990(c, m, d, __VA_ARGS__)
#define _ENUM_M992(c, m, d, x, ...) m(d, c-992, x) _ENUM_M991(c, m, d, __VA_ARGS__)
#define _ENUM_M993(c, m, d, x, ...) m(d, c-993, x) _ENUM_M992(c, m, d, __VA_ARGS__)
#define _ENUM_M994(c, m, d, x, ...) m(d, c-994, x) _ENUM_M993(c, m, d, __VA_ARGS__)
#define _ENUM_M995(c, m, d, x, ...) m(d, c-995, x) _ENUM_M994(c, m, d, __VA_ARGS__)
#define _ENUM_M996(c, m, d, x, ...) m(d, c-996, x) _ENUM_M995(c, m, d, __VA_ARGS__)
#define _ENUM_M997(c, m, d, x, ...) m(d, c-997, x) _ENUM_M996(c, m, d, __VA_ARGS__)
#define _ENUM_M998(c, m, d, x, ...) m(d, c-998, x) _ENUM_M997(c, m, d, __VA_ARGS__)
#define _ENUM_M999(c, m, d, x, ...) m(d, c-999, x) _ENUM_M998(c, m, d, __VA_ARGS__)

#define _ENUM_COUNT(_1,   _2,   _3,   _4,   _5,   _6,   _7,   _8,   _9, \
	_10,  _11,  _12,  _13,  _14,  _15,  _16,  _17,  _18,  _19,  _20,  _21,  _22, \
	_23,  _24,  _25,  _26,  _27,  _28,  _29,  _30,  _31,  _32,  _33,  _34,  _35, \
	_36,  _37,  _38,  _39,  _40,  _41,  _42,  _43,  _44,  _45,  _46,  _47,  _48, \
	_49,  _50,  _51,  _52,  _53,  _54,  _55,  _56,  _57,  _58,  _59,  _60,  _61, \
	_62,  _63,  _64,  _65,  _66,  _67,  _68,  _69,  _70,  _71,  _72,  _73,  _74, \
	_75,  _76,  _77,  _78,  _79,  _80,  _81,  _82,  _83,  _84,  _85,  _86,  _87, \
	_88,  _89,  _90,  _91,  _92,  _93,  _94,  _95,  _96,  _97,  _98,  _99,  _100, \
	_101, _102, _103, _104, _105, _106, _107, _108, _109, _110, _111, _112, _113, \
	_114, _115, _116, _117, _118, _119, _120, _121, _122, _123, _124, _125, _126, \
	_127, _128, _129, _130, _131, _132, _133, _134, _135, _136, _137, _138, _139, \
	_140, _141, _142, _143, _144, _145, _146, _147, _148, _149, _150, _151, _152, \
	_153, _154, _155, _156, _157, _158, _159, _160, _161, _162, _163, _164, _165, \
	_166, _167, _168, _169, _170, _171, _172, _173, _174, _175, _176, _177, _178, \
	_179, _180, _181, _182, _183, _184, _185, _186, _187, _188, _189, _190, _191, \
	_192, _193, _194, _195, _196, _197, _198, _199, _200, _201, _202, _203, _204, \
	_205, _206, _207, _208, _209, _210, _211, _212, _213, _214, _215, _216, _217, \
	_218, _219, _220, _221, _222, _223, _224, _225, _226, _227, _228, _229, _230, \
	_231, _232, _233, _234, _235, _236, _237, _238, _239, _240, _241, _242, _243, \
	_244, _245, _246, _247, _248, _249, _250, _251, _252, _253, _254, _255, _256, \
	_257, _258, _259, _260, _261, _262, _263, _264, _265, _266, _267, _268, _269, \
	_270, _271, _272, _273, _274, _275, _276, _277, _278, _279, _280, _281, _282, \
	_283, _284, _285, _286, _287, _288, _289, _290, _291, _292, _293, _294, _295, \
	_296, _297, _298, _299, _300, _301, _302, _303, _304, _305, _306, _307, _308, \
	_309, _310, _311, _312, _313, _314, _315, _316, _317, _318, _319, _320, _321, \
	_322, _323, _324, _325, _326, _327, _328, _329, _330, _331, _332, _333, _334, \
	_335, _336, _337, _338, _339, _340, _341, _342, _343, _344, _345, _346, _347, \
	_348, _349, _350, _351, _352, _353, _354, _355, _356, _357, _358, _359, _360, \
	_361, _362, _363, _364, _365, _366, _367, _368, _369, _370, _371, _372, _373, \
	_374, _375, _376, _377, _378, _379, _380, _381, _382, _383, _384, _385, _386, \
	_387, _388, _389, _390, _391, _392, _393, _394, _395, _396, _397, _398, _399, \
	_400, _401, _402, _403, _404, _405, _406, _407, _408, _409, _410, _411, _412, \
	_413, _414, _415, _416, _417, _418, _419, _420, _421, _422, _423, _424, _425, \
	_426, _427, _428, _429, _430, _431, _432, _433, _434, _435, _436, _437, _438, \
	_439, _440, _441, _442, _443, _444, _445, _446, _447, _448, _449, _450, _451, \
	_452, _453, _454, _455, _456, _457, _458, _459, _460, _461, _462, _463, _464, \
	_465, _466, _467, _468, _469, _470, _471, _472, _473, _474, _475, _476, _477, \
	_478, _479, _480, _481, _482, _483, _484, _485, _486, _487, _488, _489, _490, \
	_491, _492, _493, _494, _495, _496, _497, _498, _499, _500, _501, _502, _503, \
	_504, _505, _506, _507, _508, _509, _510, _511, _512, _513, _514, _515, _516, \
	_517, _518, _519, _520, _521, _522, _523, _524, _525, _526, _527, _528, _529, \
	_530, _531, _532, _533, _534, _535, _536, _537, _538, _539, _540, _541, _542, \
	_543, _544, _545, _546, _547, _548, _549, _550, _551, _552, _553, _554, _555, \
	_556, _557, _558, _559, _560, _561, _562, _563, _564, _565, _566, _567, _568, \
	_569, _570, _571, _572, _573, _574, _575, _576, _577, _578, _579, _580, _581, \
	_582, _583, _584, _585, _586, _587, _588, _589, _590, _591, _592, _593, _594, \
	_595, _596, _597, _598, _599, _600, _601, _602, _603, _604, _605, _606, _607, \
	_608, _609, _610, _611, _612, _613, _614, _615, _616, _617, _618, _619, _620, \
	_621, _622, _623, _624, _625, _626, _627, _628, _629, _630, _631, _632, _633, \
	_634, _635, _636, _637, _638, _639, _640, _641, _642, _643, _644, _645, _646, \
	_647, _648, _649, _650, _651, _652, _653, _654, _655, _656, _657, _658, _659, \
	_660, _661, _662, _663, _664, _665, _666, _667, _668, _669, _670, _671, _672, \
	_673, _674, _675, _676, _677, _678, _679, _680, _681, _682, _683, _684, _685, \
	_686, _687, _688, _689, _690, _691, _692, _693, _694, _695, _696, _697, _698, \
	_699, _700, _701, _702, _703, _704, _705, _706, _707, _708, _709, _710, _711, \
	_712, _713, _714, _715, _716, _717, _718, _719, _720, _721, _722, _723, _724, \
	_725, _726, _727, _728, _729, _730, _731, _732, _733, _734, _735, _736, _737, \
	_738, _739, _740, _741, _742, _743, _744, _745, _746, _747, _748, _749, _750, \
	_751, _752, _753, _754, _755, _756, _757, _758, _759, _760, _761, _762, _763, \
	_764, _765, _766, _767, _768, _769, _770, _771, _772, _773, _774, _775, _776, \
	_777, _778, _779, _780, _781, _782, _783, _784, _785, _786, _787, _788, _789, \
	_790, _791, _792, _793, _794, _795, _796, _797, _798, _799, _800, _801, _802, \
	_803, _804, _805, _806, _807, _808, _809, _810, _811, _812, _813, _814, _815, \
	_816, _817, _818, _819, _820, _821, _822, _823, _824, _825, _826, _827, _828, \
	_829, _830, _831, _832, _833, _834, _835, _836, _837, _838, _839, _840, _841, \
	_842, _843, _844, _845, _846, _847, _848, _849, _850, _851, _852, _853, _854, \
	_855, _856, _857, _858, _859, _860, _861, _862, _863, _864, _865, _866, _867, \
	_868, _869, _870, _871, _872, _873, _874, _875, _876, _877, _878, _879, _880, \
	_881, _882, _883, _884, _885, _886, _887, _888, _889, _890, _891, _892, _893, \
	_894, _895, _896, _897, _898, _899, _900, _901, _902, _903, _904, _905, _906, \
	_907, _908, _909, _910, _911, _912, _913, _914, _915, _916, _917, _918, _919, \
	_920, _921, _922, _923, _924, _925, _926, _927, _928, _929, _930, _931, _932, \
	_933, _934, _935, _936, _937, _938, _939, _940, _941, _942, _943, _944, _945, \
	_946, _947, _948, _949, _950, _951, _952, _953, _954, _955, _956, _957, _958, \
	_959, _960, _961, _962, _963, _964, _965, _966, _967, _968, _969, _970, _971, \
	_972, _973, _974, _975, _976, _977, _978, _979, _980, _981, _982, _983, _984, \
	_985, _986, _987, _988, _989, _990, _991, _992, _993, _994, _995, _996, _997, \
	_998, _999, count, ...) count

#define ENUM_COUNT(...) _ENUM_COUNT(__VA_ARGS__, \
	999,  998,  997,  996,  995,  994,  993,  992,  991,  990,  989,  988,  987, \
	986,  985,  984,  983,  982,  981,  980,  979,  978,  977,  976,  975,  974, \
	973,  972,  971,  970,  969,  968,  967,  966,  965,  964,  963,  962,  961, \
	960,  959,  958,  957,  956,  955,  954,  953,  952,  951,  950,  949,  948, \
	947,  946,  945,  944,  943,  942,  941,  940,  939,  938,  937,  936,  935, \
	934,  933,  932,  931,  930,  929,  928,  927,  926,  925,  924,  923,  922, \
	921,  920,  919,  918,  917,  916,  915,  914,  913,  912,  911,  910,  909, \
	908,  907,  906,  905,  904,  903,  902,  901,  900,  899,  898,  897,  896, \
	895,  894,  893,  892,  891,  890,  889,  888,  887,  886,  885,  884,  883, \
	882,  881,  880,  879,  878,  877,  876,  875,  874,  873,  872,  871,  870, \
	869,  868,  867,  866,  865,  864,  863,  862,  861,  860,  859,  858,  857, \
	856,  855,  854,  853,  852,  851,  850,  849,  848,  847,  846,  845,  844, \
	843,  842,  841,  840,  839,  838,  837,  836,  835,  834,  833,  832,  831, \
	830,  829,  828,  827,  826,  825,  824,  823,  822,  821,  820,  819,  818, \
	817,  816,  815,  814,  813,  812,  811,  810,  809,  808,  807,  806,  805, \
	804,  803,  802,  801,  800,  799,  798,  797,  796,  795,  794,  793,  792, \
	791,  790,  789,  788,  787,  786,  785,  784,  783,  782,  781,  780,  779, \
	778,  777,  776,  775,  774,  773,  772,  771,  770,  769,  768,  767,  766, \
	765,  764,  763,  762,  761,  760,  759,  758,  757,  756,  755,  754,  753, \
	752,  751,  750,  749,  748,  747,  746,  745,  744,  743,  742,  741,  740, \
	739,  738,  737,  736,  735,  734,  733,  732,  731,  730,  729,  728,  727, \
	726,  725,  724,  723,  722,  721,  720,  719,  718,  717,  716,  715,  714, \
	713,  712,  711,  710,  709,  708,  707,  706,  705,  704,  703,  702,  701, \
	700,  699,  698,  697,  696,  695,  694,  693,  692,  691,  690,  689,  688, \
	687,  686,  685,  684,  683,  682,  681,  680,  679,  678,  677,  676,  675, \
	674,  673,  672,  671,  670,  669,  668,  667,  666,  665,  664,  663,  662, \
	661,  660,  659,  658,  657,  656,  655,  654,  653,  652,  651,  650,  649, \
	648,  647,  646,  645,  644,  643,  642,  641,  640,  639,  638,  637,  636, \
	635,  634,  633,  632,  631,  630,  629,  628,  627,  626,  625,  624,  623, \
	622,  621,  620,  619,  618,  617,  616,  615,  614,  613,  612,  611,  610, \
	609,  608,  607,  606,  605,  604,  603,  602,  601,  600,  599,  598,  597, \
	596,  595,  594,  593,  592,  591,  590,  589,  588,  587,  586,  585,  584, \
	583,  582,  581,  580,  579,  578,  577,  576,  575,  574,  573,  572,  571, \
	570,  569,  568,  567,  566,  565,  564,  563,  562,  561,  560,  559,  558, \
	557,  556,  555,  554,  553,  552,  551,  550,  549,  548,  547,  546,  545, \
	544,  543,  542,  541,  540,  539,  538,  537,  536,  535,  534,  533,  532, \
	531,  530,  529,  528,  527,  526,  525,  524,  523,  522,  521,  520,  519, \
	518,  517,  516,  515,  514,  513,  512,  511,  510,  509,  508,  507,  506, \
	505,  504,  503,  502,  501,  500,  499,  498,  497,  496,  495,  494,  493, \
	492,  491,  490,  489,  488,  487,  486,  485,  484,  483,  482,  481,  480, \
	479,  478,  477,  476,  475,  474,  473,  472,  471,  470,  469,  468,  467, \
	466,  465,  464,  463,  462,  461,  460,  459,  458,  457,  456,  455,  454, \
	453,  452,  451,  450,  449,  448,  447,  446,  445,  444,  443,  442,  441, \
	440,  439,  438,  437,  436,  435,  434,  433,  432,  431,  430,  429,  428, \
	427,  426,  425,  424,  423,  422,  421,  420,  419,  418,  417,  416,  415, \
	414,  413,  412,  411,  410,  409,  408,  407,  406,  405,  404,  403,  402, \
	401,  400,  399,  398,  397,  396,  395,  394,  393,  392,  391,  390,  389, \
	388,  387,  386,  385,  384,  383,  382,  381,  380,  379,  378,  377,  376, \
	375,  374,  373,  372,  371,  370,  369,  368,  367,  366,  365,  364,  363, \
	362,  361,  360,  359,  358,  357,  356,  355,  354,  353,  352,  351,  350, \
	349,  348,  347,  346,  345,  344,  343,  342,  341,  340,  339,  338,  337, \
	336,  335,  334,  333,  332,  331,  330,  329,  328,  327,  326,  325,  324, \
	323,  322,  321,  320,  319,  318,  317,  316,  315,  314,  313,  312,  311, \
	310,  309,  308,  307,  306,  305,  304,  303,  302,  301,  300,  299,  298, \
	297,  296,  295,  294,  293,  292,  291,  290,  289,  288,  287,  286,  285, \
	284,  283,  282,  281,  280,  279,  278,  277,  276,  275,  274,  273,  272, \
	271,  270,  269,  268,  267,  266,  265,  264,  263,  262,  261,  260,  259, \
	258,  257,  256,  255,  254,  253,  252,  251,  250,  249,  248,  247,  246, \
	245,  244,  243,  242,  241,  240,  239,  238,  237,  236,  235,  234,  233, \
	232,  231,  230,  229,  228,  227,  226,  225,  224,  223,  222,  221,  220, \
	219,  218,  217,  216,  215,  214,  213,  212,  211,  210,  209,  208,  207, \
	206,  205,  204,  203,  202,  201,  200,  199,  198,  197,  196,  195,  194, \
	193,  192,  191,  190,  189,  188,  187,  186,  185,  184,  183,  182,  181, \
	180,  179,  178,  177,  176,  175,  174,  173,  172,  171,  170,  169,  168, \
	167,  166,  165,  164,  163,  162,  161,  160,  159,  158,  157,  156,  155, \
	154,  153,  152,  151,  150,  149,  148,  147,  146,  145,  144,  143,  142, \
	141,  140,  139,  138,  137,  136,  135,  134,  133,  132,  131,  130,  129, \
	128,  127,  126,  125,  124,  123,  122,  121,  120,  119,  118,  117,  116, \
	115,  114,  113,  112,  111,  110,  109,  108,  107,  106,  105,  104,  103, \
	102,  101,  100,  99,   98,   97,   96,   95,   94,   93,   92,   91,   90, \
	89,   88,   87,   86,   85,   84,   83,   82,   81,   80,   79,   78,   77, \
	76,   75,   74,   73,   72,   71,   70,   69,   68,   67,   66,   65,   64, \
	63,   62,   61,   60,   59,   58,   57,   56,   55,   54,   53,   52,   51, \
	50,   49,   48,   47,   46,   45,   44,   43,   42,   41,   40,   39,   38, \
	37,   36,   35,   34,   33,   32,   31,   30,   29,   28,   27,   26,   25, \
	24,   23,   22,   21,   20,   19,   18,   17,   16,   15,   14,   13,   12, \
	11,   10,   9,    8,    7,    6,    5,    4,    3,    2,    1)

#define _ENUM_APPLY(macro, ...) macro(__VA_ARGS__)

#define _ENUM_MAP(macro, data, ...) \
	_ENUM_APPLY(_ENUM_MAP_VAR_COUNT, ENUM_COUNT(__VA_ARGS__))(ENUM_COUNT(__VA_ARGS__), macro, data, __VA_ARGS__)
#define _ENUM_MAP_VAR_COUNT(count) _ENUM_M ## count

//

#define _ENUM_IS_EMPTY_CASE_0001 ,
#define _ENUM_TRIGGER_PARENTHESIS_(...) ,
#define _ENUM_TRUE_FALSE(_0, _1, _2, ...) _2
#define _ENUM_PASTE(_0, _1, _2, _3, _4) _0 ## _1 ## _2 ## _3 ## _4
#define _ENUM_HAS_COMMA(fallback, value, ...) _ENUM_TRUE_FALSE(__VA_ARGS__, fallback, value)
#define _ENUM_FALLBACK(fallback, value, _0, _1, _2, _3) _ENUM_HAS_COMMA(fallback, value, _ENUM_PASTE(_ENUM_IS_EMPTY_CASE_, _0, _1, _2, _3))
#define ENUM_FALLBACK(fallback, value, ...) _ENUM_FALLBACK(fallback, value, \
	_ENUM_HAS_COMMA(1, 0, __VA_ARGS__), \
	_ENUM_HAS_COMMA(1, 0, _ENUM_TRIGGER_PARENTHESIS_ __VA_ARGS__), \
	_ENUM_HAS_COMMA(1, 0, __VA_ARGS__ (/*empty*/)), \
	_ENUM_HAS_COMMA(1, 0, _ENUM_TRIGGER_PARENTHESIS_ __VA_ARGS__ (/*empty*/)) \
)
#define _ENUM_EMPTY(...)

//

#define _ENUM_TO_EXPRESSION(expression) \
	expression,

#define _ENUM_TO_SINGLE_EXPRESSION(Enum, index, expression) \
	ENUM_FALLBACK(_ENUM_EMPTY, _ENUM_TO_EXPRESSION, expression)(expression)


#ifdef __cplusplus

#define _ENUM_EMPTY_ARRAY_VALUE(Enum, index, expression) \
	(Enum)(0),

#define _ENUM_ARRAY_VALUE(Enum, index, expression) \
	(Enum)((_enum_value<Enum>)Enum::expression),

#define _ENUM_TO_SINGLE_ARRAY_VALUE(Enum, index, expression) \
	ENUM_FALLBACK(_ENUM_EMPTY_ARRAY_VALUE, _ENUM_ARRAY_VALUE, expression)(Enum, index, expression)

#define _ENUM_EMPTY_ARRAY_NAME(Enum, index, expression) \
	std::string_view(),

#define _ENUM_ARRAY_NAME(Enum, index, expression) \
	std::string_view(#expression, _enum_name_length(#expression)),

#define _ENUM_TO_SINGLE_ARRAY_NAME(Enum, index, expression) \
	ENUM_FALLBACK(_ENUM_EMPTY_ARRAY_NAME, _ENUM_ARRAY_NAME, expression)(Enum, index, expression)

#define _ENUM_PHF_NAME(Enum, index, expression) \
	hhl(detail_ ## Enum::names[index]),

#define _ENUM_TO_SINGLE_PHF_NAME(Enum, index, expression) \
	ENUM_FALLBACK(_ENUM_EMPTY, _ENUM_PHF_NAME, expression)(Enum, index, expression)

#define _ENUM_PHF_VALUE(Enum, index, expression) \
	hmix(static_cast<detail_ ## Enum::type>(detail_ ## Enum::values[index])),

#define _ENUM_TO_SINGLE_PHF_VALUE(Enum, index, expression) \
	ENUM_FALLBACK(_ENUM_EMPTY, _ENUM_PHF_VALUE, expression)(Enum, index, expression)

#define _ENUM_PHF_VALUES_CASE(Enum, index, expression) \
	case enum_find(detail_ ## Enum::values[index]): \
		return detail_ ## Enum::names[index];

#define _ENUM_TO_SINGLE_PHF_VALUES_CASE(Enum, index, expression) \
	ENUM_FALLBACK(_ENUM_EMPTY, _ENUM_PHF_VALUES_CASE, expression)(Enum, index, expression)

#define _ENUM_PHF_NAMES_CASE(Enum, index, expression) \
	case enum_find<Enum>(detail_ ## Enum::names[index]): \
		return detail_ ## Enum::values[index];

#define _ENUM_TO_SINGLE_PHF_NAMES_CASE(Enum, index, expression) \
	ENUM_FALLBACK(_ENUM_EMPTY, _ENUM_PHF_NAMES_CASE, expression)(Enum, index, expression)

#include <string_view>

#include "phf.hh"
#include "hashes.hh"

constexpr inline bool _enum_name_ends(char c, std::size_t index = 0) {
	constexpr const char *_name_enders = "= \t\n";
	return c == _name_enders[index] ? true : _name_enders[index] == '\0' ? false : _enum_name_ends(c, index + 1);
}

constexpr inline std::size_t _enum_name_length(const char *s, std::size_t index = 0) {
	return _enum_name_ends(s[index]) ? index : _enum_name_length(s, index + 1);
}

template <typename Enum>
class _enum_value {
	Enum value;
public:
	explicit constexpr _enum_value(Enum value) : value(value) {}
	template <typename Any> constexpr const _enum_value& operator=(Any) const { return *this; }
	constexpr operator Enum() const { return value; }
};

template <typename T>
constexpr inline auto enum_find(std::string_view);
template <typename T>
constexpr inline T enum_type(std::string_view);

#define _ENUM_IMPL(Enum, Underlying, ...) \
	namespace detail_ ## Enum { \
		typedef Underlying type; \
		enum _putNamesInThisScope : type { \
			_ENUM_MAP(_ENUM_TO_SINGLE_EXPRESSION, Enum, __VA_ARGS__) \
		}; \
		constexpr const Enum values[] = { \
			_ENUM_MAP(_ENUM_TO_SINGLE_ARRAY_VALUE, Enum, __VA_ARGS__) \
		}; \
		constexpr const std::string_view names[] = { \
			_ENUM_MAP(_ENUM_TO_SINGLE_ARRAY_NAME, Enum, __VA_ARGS__) \
		}; \
	} \
	constexpr inline auto enum_find(Enum type) { \
		constexpr auto _phf_values = phf::make_phf({ \
			_ENUM_MAP(_ENUM_TO_SINGLE_PHF_VALUE, Enum, __VA_ARGS__) \
		}); \
		return _phf_values.fhmix(static_cast<detail_ ## Enum::type>(type)); \
	} \
	constexpr inline std::string_view enum_name(Enum type) { \
		switch (enum_find(type)) { \
			_ENUM_MAP(_ENUM_TO_SINGLE_PHF_VALUES_CASE, Enum, __VA_ARGS__) \
			default: \
				return ""; \
		} \
	} \
	template <> \
	constexpr inline auto enum_find<Enum>(std::string_view name) { \
		constexpr auto _phf_names = phf::make_phf({ \
			_ENUM_MAP(_ENUM_TO_SINGLE_PHF_NAME, Enum, __VA_ARGS__) \
		}); \
		return _phf_names.fhhl(name); \
	} \
	template <> \
	constexpr inline Enum enum_type<Enum>(std::string_view name) { \
		switch (enum_find<Enum>(name)) { \
			_ENUM_MAP(_ENUM_TO_SINGLE_PHF_NAMES_CASE, Enum, __VA_ARGS__) \
			default: \
				throw std::out_of_range(""); \
		} \
	}

#define ENUM(Enum, Underlying, ...) \
	enum class Enum : Underlying { \
		_ENUM_MAP(_ENUM_TO_SINGLE_EXPRESSION, Enum, __VA_ARGS__) \
	}; \
	_ENUM_IMPL(Enum, Underlying, __VA_ARGS__)


#define ENUM_C(Enum, ...) \
	extern "C" { \
	enum Enum { \
		_ENUM_MAP(_ENUM_TO_SINGLE_EXPRESSION, Enum, __VA_ARGS__) \
	}; \
	} \
	_ENUM_IMPL(Enum, int, __VA_ARGS__)

#else // __cplusplus

#define ENUM_C(Enum, ...) \
	enum Enum { \
		_ENUM_MAP(_ENUM_TO_SINGLE_EXPRESSION, Enum, __VA_ARGS__) \
	};

#endif // __cplusplus

#endif // ENUM_H
