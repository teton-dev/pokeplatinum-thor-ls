#ifndef POKEPLATINUM_PORT_CAMERA_TILT_H
#define POKEPLATINUM_PORT_CAMERA_TILT_H

// Camera tilt (Settings > Display > Camera tilt): the overworld camera a few degrees lower, between Platinum's nearly
// top-down view and Black 2 and White 2's, so a little more of what lies ahead shows. Applied only while the field map is
// drawn (the camera is tilted before its view matrix is made and put back after), so nothing else that reads or sets the
// camera's angle (cut scenes, encounter effects) sees it.

#include "field/field_system_decl.h"

#ifdef __cplusplus
extern "C" {
#endif

void CameraTilt_Apply(FieldSystem *fieldSystem);
void CameraTilt_Restore(FieldSystem *fieldSystem);

#ifdef __cplusplus
}
#endif

#endif // POKEPLATINUM_PORT_CAMERA_TILT_H
