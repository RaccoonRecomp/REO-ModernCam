#ifndef __REO_CAMERA_US_H__
#define __REO_CAMERA_US_H__

// Resident Evil Outbreak (NTSC-U), SLUS-20765 disc version 2.00: every game DATA address this mod uses.
//
// Outbreak ModernCam was written for the Japanese disc (Biohazard Outbreak, SLPM-65428). Each address below is its
// Japanese address (in brackets) moved to this disc by matching the code that uses it in SLUS_207.65 and the
// BIN\*.DAT overlays. The shifts are not uniform, so every address was found on its own; the evidence is noted next
// to it. Code addresses in 0x0057xxxx-0x0070xxxx are in game.bin (BIN\2.DAT, loaded at 0x00570000, code
// 0x00570040-0x00708580); 0x0037xxxx is submain.bin (BIN\0.DAT, 0x00370000).
//
// v2: the mod never reads or writes game CODE. The code sites ModernCam patches (the camera freeze and free aim) are
// listed in patches.json, compiled into the recompiled game as switchable feature sites, and switched with
// reo_feature_set() (reo_host.h). Everything here is data.

// ---- Camera block (executable bss; Japanese + 0x1DE00) ------------------------------------------
// Eye x,y,z then look-at x,y,z (6 f32). The fixed-camera handler 0x005BF1F0 copies each cut's
// eye (+0xA0) and target (+0xB8) here; 0x005D9960 writes both through 0x00370500. [0x00306338]
#define REO_US_CAM_EYE     0x00324138
#define REO_US_CAM_TARGET  0x00324144
// f32, 500.0 after the camera init at 0x00375650. ModernCam's "near" value [0x00306374]. On this disc it is the
// fixed-cut handler's look distance (0x005BF290: target = eye + R * (0, 0, -value)); the projection's near plane is
// 0x0032417C (1.0) and is not touched. ModernCam writes this word, so the port does too.
#define REO_US_CAM_NEAR    0x00324174
// f32, 50000.0 after init: the projection's far plane (0x003757B8 passes it as the first operand). [0x00306378]
#define REO_US_CAM_FAR     0x00324178
// f32 vertical FOV in degrees: set from each cut's +0x14 by 0x005BF1F0, read by the projection
// setup at 0x00375778. [0x00306380]
#define REO_US_CAM_FOV_DEG 0x00324180
// f32 4/3: the projection's aspect operand (0x003757C8), ModernCam's horizontal scale "C_FOV".
// The Japanese disc keeps it in the executable's data [0x00239D48]; this disc keeps it in
// submain.bin's data, so it was identified by use, not by a shift.
#define REO_US_CAM_ASPECT  0x0038CA78

// Character light: R,G,B,A bytes, then a mode byte (written 2 to force it). Same block as the camera.
// [0x00306298, 0x003062BC]
#define REO_US_ENTITY_LIGHT      0x00324098
#define REO_US_ENTITY_LIGHT_MODE 0x003240BC

// ---- Pad block (executable bss; Japanese + 0x1DE30) ---------------------------------------------
// u32 copy of the vblank counter, taken at each game-frame sync (0x001BAB44). [0x003065C4]
#define REO_US_FRAMES       0x003243F4
// u8 control type 0-3; the button mapper loads it at 0x001B0C58. [0x003065BC]
#define REO_US_CONTROL_TYPE 0x003243EC
// Mapped pad records, 48 bytes each, filled by 0x001B06F0 and read by the mapper at 0x001B0B80.
//   +0x08 u32 held buttons (the mapper's source word, see REO_SRC_*)             [RAW_BTN_WORD 0x00306418]
//   +0x20..+0x27 left stick  (s16 magnitude, s16 angle - 0x4000, s16 x, s16 y)    [ANALOG_BLOCK 0x00306434]
//   +0x28..+0x2F right stick (same layout)
// The mapper loads +0x08 at 0x001B0C5C (lw t4,0(a2), a2 = the record's +0x08: ModernCam's VBTN hook [0x001AFD38])
// and only after that copies +0x20..+0x2F to the same offsets of the character's action block, so a change made at
// the camera.padMap mark (patches.json) applies exactly in that game frame.
#define REO_US_PAD_REC      0x00324240
#define REO_US_PAD_REC_SIZE 48
#define REO_PADREC_HELD     0x08
#define REO_PADREC_LSTICK   0x20
#define REO_PADREC_RSTICK   0x28
// u32 action bits per control type for [square, cross, circle] (12 bytes per type). [0x002168C0]
#define REO_US_BTN_MAP_TABLE 0x002343C0

// ---- Raw DualShock frames (libpad2) -------------------------------------------------------------
// Socket 0's frame-buffer pointer (socket table 0x00276188, 0x334 bytes per socket); the frame getter
// 0x00180970 reads it, picks the newer of the two 0x80-byte frames by the counter at +0x7C, and
// scePad2Read (0x001804D8) copies the pad data from +0x1C. [PAD_PTR 0x00258698 = table + 0x10]
// v2 reads the game's pad here only when the host gives no camera input (reo_cam_input() returns 0).
#define REO_US_PAD_PTR        0x00276198
#define REO_US_PADFRAME_SIZE  0x80
#define REO_US_PADFRAME_COUNT 0x7C // u32 frame counter
#define REO_US_PADFRAME_LEN   0x02 // u8 pad-data length
#define REO_US_PADFRAME_BTN   0x1C // u16 SCE buttons, active-low
#define REO_US_PADFRAME_RX    0x1E // u8 right stick x (0x80 = centre)
#define REO_US_PADFRAME_RY    0x1F
#define REO_US_PADFRAME_LX    0x20
#define REO_US_PADFRAME_LY    0x21

// ---- Characters (submain.bin bss; Japanese + 0x2EE60) -------------------------------------------
// Four 0x10E0-byte records; the camera handlers index them at 0x005BF304 and 0x005D9964. [0x00476DD0]
// Offline the player is record 0 in every scenario (0x003DDDD4, the index the camera handlers follow, stays 0 in
// the Outbreak, Hellfire, The Hive, Below Freezing Point and Decisions, Decisions probes).
#define REO_US_CHAR_BASE   0x004A5C30
#define REO_US_CHAR_STRIDE 0x10E0
// Offsets inside a record (unchanged from the Japanese disc: +0x92 is read by the handler at
// 0x005D9A8C, +0xBC8 by the free-aim sites, +0xF78..+0xF88 and +0xFF0 by the mapper, +0xC7C by the item code).
#define REO_CHAR_STANCE    0x008  // u8, 2 = weapon raised
#define REO_CHAR_SUBSTATE  0x009  // u8, 0x40 = scripted action (the reload animation 0x006690E0 starts)
#define REO_CHAR_POS       0x038  // f32 x,y,z
#define REO_CHAR_FWD_X     0x070  // f32 model-matrix row 2 x
#define REO_CHAR_FWD_Z     0x078  // f32 model-matrix row 2 z
#define REO_CHAR_HEADING   0x092  // s16, 0x10000 per turn
#define REO_CHAR_ACTION    0x54F  // u8 action; 0x1C while a combine/reload runs (set by 0x006690E0)
#define REO_CHAR_AIM_ANGLE 0xBC8  // s16 gun elevation, 65536/360 per degree, +-0x1FFF
#define REO_CHAR_EQUIPPED  0xC7C  // u8 item-table slot of the equipped weapon, 0 = none [EQUIPPED_OFF 3196]
#define REO_CHAR_ACT       0xF78  // action block: +0 pressed, +8 held, +0x20..+0x2F stick copy
#define REO_CHAR_AI_GATE   0xFF0  // u8, 0 = player-driven
#define REO_AIM_ANGLE_MAX  0x1FFF
// u32, the aim target (the game's auto-aim lock): when she raises the weapon with the aim action, the aim-stance entry
// 0x0064E5C0 stores the target finder's result (0x0065F060 -> 0x0065ED70: the nearest enemy in range and in front of
// her, 0 = none) here and sets the stance to 2. The word is not cleared when the aim ends: it counts only while she
// aims (stance 2) or takes a hit while aiming (stance 3, 0x00661074; when it ends the game aims and looks again).
#define REO_STANCE_AIM     2
#define REO_STANCE_AIM_HIT 3
#define REO_CHAR_AIM_TARGET 0x024

// ---- Enemies (submain.bin bss) ---------------------------------------------------------------------------------
// The records the target finder walks: 0x0042EB00 + index * 0xD90 (0x0065EEC0), up to the character records. The finder
// skips a record whose +0xC94 bit 0 is set; 0x0065F0C0 also needs +0 (u8 live) and +0x34 (its model) non-zero.
#define REO_US_ENEMY_BASE   0x0042EB00
#define REO_US_ENEMY_STRIDE 0xD90
#define REO_ENEMY_LIVE      0x000
#define REO_ENEMY_MODEL     0x034
#define REO_ENEMY_POS       0x038  // f32 x,y,z (as a character record's)
#define REO_ENEMY_FLAGS     0xC94
#define REO_ENEMY_UNTARGETABLE 0x1

// ---- Camera cuts (submain.bin bss; Japanese + 0x2EE60) ------------------------------------------
// Header: +1 u8 cut count, +3 u8 current cut, +5 u8 forced cut, +6 u8 force flag; records of 0x198
// bytes from +0x34 with the authored eye (3 x s32) at +0xA0 and target at +0xB8. The cut dispatcher
// 0x005BED30 reads the header, the force index (+5) and flag (+6). [0x003AEF50]
#define REO_US_CUT_HEADER 0x003DDDB0
#define REO_CUT_COUNT     1
#define REO_CUT_CURRENT   3
#define REO_CUT_FORCE_IDX 5
#define REO_CUT_FORCE_ON  6
#define REO_CUT_RECORDS   0x34
#define REO_CUT_STRIDE    0x198
#define REO_CUT_FOV       0x14
#define REO_CUT_EYE       0xA0
#define REO_CUT_TARGET    0xB8
// The room objects a cut draws: 4 words, bit i = object i. At every cut change the dispatcher sets bit 0 of each
// object's flag byte (REO_US_OBJ_FLAGS) from the new cut's mask (game.bin 0x005BEF3C, after it stores +3).
#define REO_CUT_MASK       0x154
#define REO_CUT_MASK_WORDS 4

// ---- Objects, walls, near-camera hide radii -----------------------------------------------------
// u8 per room object, bit 0 = visible; game.bin indexes it from 0x00690614 on. [0x00470930]
#define REO_US_OBJ_FLAGS 0x0049F790
// u16 object count, 0x2E before the flags as on the Japanese disc (same +0x2EE60 shift). [0x00470902]
#define REO_US_OBJ_COUNT 0x0049F762
// f32 4x4 world matrix per room object (rows; the origin at +0x30).
#define REO_US_OBJ_MATRIX      0x0049FA10
#define REO_OBJ_MATRIX_STRIDE  0x40
#define REO_OBJ_MATRIX_ORIGIN  0x30
// ---- The room's draw (submain.bin's room loop 0x00372210; the mirror draws with the game's own functions) ----------
// The room's draw data: the loop's a0 ($s3 at the camera.roomDraw mark). REO_US_OBJ_COUNT, REO_US_OBJ_FLAGS and
// REO_US_OBJ_MATRIX lie in it (+0x872, +0x8A0, +0xB20). The loop draws group g's objects (first +0x2B20 + g, count +4 +
// g; +3 the groups) whose flag has bit 0 and not bit 7: the matrix (+0xB20 + 0x40 i, or the pointer at +0x2C80 + 4 i
// when bit i of the words at +0x2C70 is set) + the vector at +0x880 into slot 0x1A, the model entry's render states
// (0x00374B20 when its +0x88 byte is set), its materials into slots 0x3A + k, then the model (0x0018C170).
#define REO_US_ROOM_DRAW      0x0049EEF0
#define REO_ROOM_GROUPS       0x003   // u8: object groups
#define REO_ROOM_GROUP_COUNT  0x004   // u8 per group: its objects
#define REO_ROOM_MATERIAL     0x068   // u16 per material index: the number of its material record
#define REO_ROOM_MODELS       0x868   // u32: the model table, REO_MODEL_STRIDE bytes per object
#define REO_ROOM_OFFSET       0x880   // f32 x3: added to every object's origin
#define REO_ROOM_RSTATES      0x920   // u32 per object: its render states
#define REO_ROOM_MATRIX       0xB20   // f32 4x4 per object (= REO_US_OBJ_MATRIX)
#define REO_ROOM_GROUP_FIRST  0x2B20  // u8 per group: its first object
#define REO_ROOM_MAT_SEL      0x2C70  // 4 words: bit i = object i's matrix is at the pointer REO_ROOM_MAT_PTR + 4 i
#define REO_ROOM_MAT_PTR      0x2C80
#define REO_MODEL_STRIDE      0x98    // +0 the model handle (0 or -1: none), +4 its material count, +8 their indices
#define REO_MODEL_COUNT       0x04    // (u32 each), +0x88 u8: the model has render states (0x00374B20 hands the
                                      // game's slots its bytes up to +0x8F; +0x8F: the depth write, 1 = on)
#define REO_MODEL_INDEX       0x08
#define REO_MODEL_HAS_RSTATES 0x88
#define REO_MODEL_ZWRITE      0x8F
// u32: the model records (0x180 bytes each), the handle's at (handle - 1) * 0x180: +0x20 its packet (a DMA "ret"
// chain per material: VIF codes, UNPACK V4-32 blocks with a GIF tag of ST, RGBAQ, XYZF2 and its vertices, 4 qwords
// each, the position first), +0x2C the chains' size, +0x40 its vertices.
#define REO_US_MODEL_RECORDS  0x002941A0
#define REO_MODEL_REC_STRIDE  0x180
#define REO_MREC_PACKET       0x20
#define REO_MREC_CHAINS_END   0x2C
#define REO_MREC_VERTICES     0x40
#define REO_US_MATERIALS      0x004EDD90 // the material records, 0x4C bytes each
#define REO_MATERIAL_STRIDE   0x4C
// exe 0x001AF780 (slot, data): slot 0x1A = the world matrix (copied to the scratchpad), 0x3A + k = material k.
#define REO_US_FN_SET_SLOT    0x001AF780
#define REO_SLOT_WORLD        0x1A
#define REO_SLOT_MATERIAL     0x3A
#define REO_US_FN_RSTATES     0x00374B20 // submain (model entry, render states)
#define REO_US_FN_DRAW_MODEL  0x0018C170 // exe (handle, 0): the model drawn with the slots; returns 1
// exe 0x00197490 (x, y, width, height, $t0 = 0 for SCISSOR_1): a GS SCISSOR packet into the frame's list from here on,
// clamped to the frame (REO_US_SCREEN_W / _H); it keeps the value at REO_US_SCISSOR_1 (x0, y0, x1, y1: u32). Returns 1.
#define REO_US_FN_SCISSOR     0x00197490
#define REO_US_SCISSOR_1      0x0029FDF8
#define REO_US_SCREEN_H       0x0029FED0 // u32 448
#define REO_US_SCREEN_W       0x0029FED8 // u32 512
// f32 4x4 (rows): the frame's view matrix, built from the camera block by 0x00375A50 before the room is drawn
// (x right, y up, the camera looks along -z). The projection: REO_US_CAM_FOV_DEG (vertical), REO_US_CAM_ASPECT.
#define REO_US_VIEW_MATRIX    0x003240E0
// u32 pointer to the room's wall records (0x38 bytes each), set by the room-collision loader
// 0x006161E0. [WALL_PTRS file1 0x003938C4]
#define REO_US_WALL_PTR  0x003C2724
// The prop flag ModernCam's cam_ignore_props skips: 0 for File 1 (ro_collision), so the setting changes nothing.
#define REO_WALL_PROP_FLAG_FILE1 0
// 4 x f32 (20 / 80 / 180 / 300), ModernCam's "draw distances" [0x006D6CF0]. On this disc the only reader is
// 0x005C0870: it returns "too close to the camera" when an object's point + 100 up is nearer to the camera eye
// than table[class]; the character, partner and enemy draw paths (0x003752D4/F8, 0x003753A0, 0x003754C0,
// 0x00591034, 0x006833F0, 0x006BA464; characters use class 1) then skip the model. These are near-camera HIDE
// radii: scaling them up hides characters near the camera (the x4 problem), so v2 never scales them above 1.
#define REO_US_CULL_TABLE 0x0070A6D0
#define REO_US_CULL_COUNT 4

// ---- Items (reload; ModernCam's find_reload / do_reload) ------------------------------------------------------------
// Item records, 256 x 60 bytes [ITEM_TABLE 0x0039E8D0]: +0 u8 present, +6 u16 item id, +0x24 u32 count,
// +0x28 u16 owner (character record + 1), +0x2B u8 type. 0x005B4380 indexes it (the cheats' ammo record 81 of
// Kevin's .45 at 0x003C692C confirms it).
#define REO_US_ITEM_TABLE   0x003C5630
#define REO_ITEM_STRIDE     60
#define REO_ITEM_SLOTS      256
#define REO_ITEM_ID         0x06
#define REO_ITEM_COUNT      0x24
#define REO_ITEM_OWNER      0x28
#define REO_ITEM_TYPE       0x2B
// Item definitions, 24 bytes per type [ITEM_DEFS 0x00716980]: +0 u8 flags (bit 1 = loose rounds, ModernCam's
// preferred ammo), +4 s16 rule offset (halfwords into the rule table), +6 s16 rule count, +8 s16 capacity,
// +0x12 u16 item id. 0x005B4380 returns this address + 24 * type (Kevin's .45 = type 18: capacity 7).
#define REO_US_ITEM_DEFS    0x0074BB50
#define REO_DEF_STRIDE      24
#define REO_DEF_FLAGS       0
#define REO_DEF_RULE_OFF    4
#define REO_DEF_RULE_N      6
#define REO_DEF_CAP         8
#define REO_DEF_FLAG_ROUNDS 0x02
// The combine rules, 6 bytes each: s16 other item id, s16 result, s16 kind. 0x005B3E40 walks def.rule_n entries
// from REO_US_ITEM_RULES + 2 * def.rule_off. (.45 type 18: .45 magazine type 55 -> kind 12, .45 rounds type 60 -> 1.)
#define REO_US_ITEM_RULES   0x0074C970
#define REO_RULE_STRIDE     6
// The item-combine routine [ITEM_COMBINE_FN 0x006375B0]: v0 = f(a0 = character record, a1 = weapon slot,
// a2 = ammo slot); 0 = the combine (the animated reload, action 0x1C, sub-state 0x40) started. ModernCam's reload
// cave calls it at the entry of the per-character update [RELOAD_HOOK 0x006399A0]; on this disc that function is
// 0x0066B500 (a0 = the character record), the camera.reload mark of patches.json. Probed: .45 empty + magazine ->
// 7 rounds; + loose rounds -> 1 round per call, as the exe.
#define REO_US_ITEM_COMBINE_FN 0x006690E0
#define REO_HOOK_RELOAD        "camera.reload"

// ---- Button bits --------------------------------------------------------------------------------
// SCE pad bits (raw frames, active-low).
#define REO_SCE_SELECT   0x0001
#define REO_SCE_L3       0x0002
#define REO_SCE_R3       0x0004
#define REO_SCE_START    0x0008
#define REO_SCE_UP       0x0010
#define REO_SCE_RIGHT    0x0020
#define REO_SCE_DOWN     0x0040
#define REO_SCE_LEFT     0x0080
#define REO_SCE_L2       0x0100
#define REO_SCE_R2       0x0200
#define REO_SCE_L1       0x0400
#define REO_SCE_R1       0x0800
#define REO_SCE_TRIANGLE 0x1000
#define REO_SCE_CIRCLE   0x2000
#define REO_SCE_CROSS    0x4000
#define REO_SCE_SQUARE   0x8000

// The mapper's source word (mapped records +0x08, active-high). Only these seven are known, from
// ModernCam's SCE_TO_SRC table; the remap table (REO_US_BTN_MAP_TABLE) confirms square/cross/circle.
#define REO_SRC_CROSS  0x0010
#define REO_SRC_CIRCLE 0x0020
#define REO_SRC_R2     0x0040
#define REO_SRC_L2     0x0080
#define REO_SRC_SQUARE 0x0100
#define REO_SRC_R1     0x0400
#define REO_SRC_L1     0x0800

// Action bits in the character's action block.
#define REO_ACT_FORWARD 0x0001
#define REO_ACT_DASH    0x0010
#define REO_ACT_FIRE    0x0020
#define REO_ACT_USE     0x0100
#define REO_ACT_AIM     0x0400 // R1: the aim-stance entry looks for a target only for this action
#define REO_ACT_MOVE    0x000F // forward 0x1, back 0x2, left 0x4, right 0x8 (held word +8): the player moves her

#endif
