#version 430

in vec2 localPos;
in vec4 passCoreColor;
in vec4 passGlowColor;
in vec2 passDirection;
in float passAngle;
in float passRadius;
flat in uint passFlags;
flat in uint passSkillId;
in float passType;
uniform float uTime;

out vec4 finalColor;

#include "skills/skill_common.glslinc"
#include "skills/skill_dispatch.glslinc"

void main() {
    NmdSkillFragParams params;
    params.localPos = localPos;
    params.coreColor = passCoreColor;
    params.glowColor = passGlowColor;
    params.direction = passDirection;
    params.angle = passAngle;
    params.radius = passRadius;
    params.type = passType;
    params.flags = passFlags;
    params.skillId = (passSkillId != 0u) ? passSkillId : NmdDecodeSkillId(passFlags);
    params.time = uTime;

    finalColor = NmdDispatchSkillFragment(params);
}
