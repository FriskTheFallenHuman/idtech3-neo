// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Doom linedef action numbers and activation modes. Shared by compiler and game.
namespace doom {
enum Kind {
    None,
    DoorRaise,
    DoorOpen,
    DoorClose,
    DoorCloseWait,
    Lift,
    PerpetualLift,
    FloorLowest,
    FloorHighest,
    FloorNext,
    FloorRaise,
    Floor24,
    Floor32,
    FloorTexture,
    FloorCrush,
    TurboLower,
    Stairs8,
    Stairs16,
    Donut,
    Crusher,
    FastCrusher,
    CeilingFloor,
    CeilingCrush,
    CeilingHighest,
    FloorCeiling,
    StopLift,
    StopCrusher,
    Teleport,
    Exit,
    SecretExit,
    LightBright,
    Light255,
    Light35,
    LightDark,
    LightStrobe,
    Scroll
};
struct Action {
    Kind kind = None;
    bool use = false, shoot = false, repeat = false, local = false;
    int key = 0;
};
inline Action action(int n) {
    Action a;
    switch (n) {
    case 1:
    case 4:
    case 26:
    case 27:
    case 28:
    case 29:
    case 63:
    case 90:
    case 105:
    case 108:
    case 111:
    case 114:
    case 117:
        a.kind = DoorRaise;
        break;
    case 2:
    case 31:
    case 32:
    case 33:
    case 34:
    case 46:
    case 61:
    case 86:
    case 103:
    case 106:
    case 109:
    case 112:
    case 115:
    case 118:
    case 99:
    case 133:
    case 134:
    case 135:
    case 136:
    case 137:
        a.kind = DoorOpen;
        break;
    case 3:
    case 42:
    case 50:
    case 75:
    case 107:
    case 110:
    case 113:
    case 116:
        a.kind = DoorClose;
        break;
    case 16:
    case 76:
        a.kind = DoorCloseWait;
        break;
    case 10:
    case 21:
    case 62:
    case 88:
    case 120:
    case 121:
    case 122:
    case 123:
        a.kind = Lift;
        break;
    case 53:
    case 87:
        a.kind = PerpetualLift;
        break;
    case 23:
    case 37:
    case 38:
    case 60:
    case 82:
    case 84:
        a.kind = FloorLowest;
        break;
    case 19:
    case 45:
    case 83:
    case 102:
        a.kind = FloorHighest;
        break;
    case 18:
    case 20:
    case 22:
    case 47:
    case 66:
    case 68:
    case 69:
    case 95:
    case 119:
    case 128:
    case 129:
    case 130:
    case 131:
    case 132:
        a.kind = FloorNext;
        break;
    case 5:
    case 64:
    case 91:
    case 101:
        a.kind = FloorRaise;
        break;
    case 15:
    case 58:
    case 59:
    case 92:
    case 93:
        a.kind = Floor24;
        break;
    case 14:
    case 67:
        a.kind = Floor32;
        break;
    case 30:
    case 96:
        a.kind = FloorTexture;
        break;
    case 55:
    case 56:
    case 65:
    case 94:
        a.kind = FloorCrush;
        break;
    case 36:
    case 70:
    case 71:
    case 98:
        a.kind = TurboLower;
        break;
    case 7:
    case 8:
        a.kind = Stairs8;
        break;
    case 100:
    case 127:
        a.kind = Stairs16;
        break;
    case 9:
        a.kind = Donut;
        break;
    case 25:
    case 49:
    case 73:
    case 141:
        a.kind = Crusher;
        break;
    case 6:
    case 77:
        a.kind = FastCrusher;
        break;
    case 41:
    case 43:
        a.kind = CeilingFloor;
        break;
    case 44:
    case 72:
        a.kind = CeilingCrush;
        break;
    case 40:
        a.kind = FloorCeiling;
        break;
    case 54:
    case 89:
        a.kind = StopLift;
        break;
    case 57:
    case 74:
        a.kind = StopCrusher;
        break;
    case 39:
    case 97:
    case 125:
    case 126:
        a.kind = Teleport;
        break;
    case 11:
    case 52:
        a.kind = Exit;
        break;
    case 51:
    case 124:
        a.kind = SecretExit;
        break;
    case 12:
    case 80:
        a.kind = LightBright;
        break;
    case 13:
    case 81:
    case 138:
        a.kind = Light255;
        break;
    case 35:
    case 79:
    case 139:
        a.kind = Light35;
        break;
    case 104:
        a.kind = LightDark;
        break;
    case 17:
        a.kind = LightStrobe;
        break;
    case 48:
        a.kind = Scroll;
        break;
    default:
        break;
    }
    switch (n) {
    case 1:
    case 26:
    case 27:
    case 28:
    case 31:
    case 32:
    case 33:
    case 34:
    case 117:
    case 118:
        a.local = true;
        break;
    default:
        break;
    }
    switch (n) {
    case 1:
    case 7:
    case 9:
    case 11:
    case 14:
    case 15:
    case 18:
    case 20:
    case 21:
    case 23:
    case 26:
    case 27:
    case 28:
    case 29:
    case 31:
    case 32:
    case 33:
    case 34:
    case 41:
    case 42:
    case 43:
    case 45:
    case 49:
    case 50:
    case 51:
    case 55:
    case 60:
    case 61:
    case 62:
    case 63:
    case 64:
    case 65:
    case 66:
    case 67:
    case 68:
    case 69:
    case 70:
    case 71:
    case 99:
    case 101:
    case 102:
    case 103:
    case 111:
    case 112:
    case 113:
    case 114:
    case 115:
    case 116:
    case 117:
    case 118:
    case 122:
    case 123:
    case 127:
    case 131:
    case 132:
    case 133:
    case 134:
    case 135:
    case 136:
    case 137:
    case 138:
    case 139:
        a.use = true;
        break;
    default:
        break;
    }
    a.shoot = n == 24 || n == 46 || n == 47;
    if (n == 24)
        a.kind = FloorRaise;
    switch (n) {
    case 1:
    case 26:
    case 27:
    case 28:
    case 42:
    case 43:
    case 45:
    case 46:
    case 60:
    case 61:
    case 62:
    case 63:
    case 64:
    case 65:
    case 66:
    case 67:
    case 68:
    case 69:
    case 70:
    case 72:
    case 73:
    case 74:
    case 75:
    case 76:
    case 77:
    case 79:
    case 80:
    case 81:
    case 82:
    case 83:
    case 84:
    case 86:
    case 87:
    case 88:
    case 89:
    case 90:
    case 91:
    case 92:
    case 93:
    case 94:
    case 95:
    case 96:
    case 97:
    case 98:
    case 99:
    case 105:
    case 106:
    case 107:
    case 114:
    case 115:
    case 116:
    case 117:
    case 120:
    case 123:
    case 126:
    case 128:
    case 129:
    case 132:
    case 134:
    case 136:
    case 138:
    case 139:
        a.repeat = true;
        break;
    default:
        break;
    }
    if (n == 26 || n == 32 || n == 99 || n == 133)
        a.key = 1;
    if (n == 27 || n == 34 || n == 136 || n == 137)
        a.key = 2;
    if (n == 28 || n == 33 || n == 134 || n == 135)
        a.key = 3;
    return a;
}
inline bool floorMoves(Kind k) {
    return (k >= Lift && k <= Donut) || k == FloorCeiling;
}
inline bool ceilingMoves(Kind k) {
    return (k >= DoorRaise && k <= DoorCloseWait) || (k >= Crusher && k <= FloorCeiling);
}
} // namespace doom
