// Camera tilt for the overworld; see include/port/camera_tilt.h
#include "port/camera_tilt.h"

#include <nitro.h>

#include "field/field_system.h"
#include "port/sim_config_prj.h"

#include "camera.h"

static BOOL sTilted = FALSE;
static CameraAngle sSavedAngle;

// Degrees of tilt for the setting's levels: off, slightly, more
static const int sTiltDegrees[] = { 0, 6, 12 };

void CameraTilt_Apply(FieldSystem *fieldSystem)
{
    sTilted = FALSE;
    const SIM_Config_prj_type *config = SIM_Config_prj_GetConfig();
    if (config == NULL || config->cameraTilt <= 0 || config->cameraTilt > 2 || fieldSystem == NULL
        || fieldSystem->camera == NULL || Camera_PortGetActive() != fieldSystem->camera
        || fieldSystem->camera->projection != CAMERA_PROJECTION_PERSPECTIVE) {
        return;
    }

    sSavedAngle = Camera_GetAngle(fieldSystem->camera);
    // The field camera looks down from a negative angle (about -59 degrees): adding to it lowers the camera toward
    // the horizon. Only a camera that looks down at between 30 and 75 degrees is tilted, so a scene that has set its own
    // angle (a vista, a cut scene) is left as it made it
    const int pitch = (s16)sSavedAngle.x; // negative: looking down
    const int low = -(75 * 0xFFFF) / 360, high = -(30 * 0xFFFF) / 360;
    if (pitch < low || pitch > high || sSavedAngle.y != 0) {
        return;
    }

    CameraAngle delta = { (u16)((sTiltDegrees[config->cameraTilt] * 0xFFFF) / 360), 0, 0, 0 };
    Camera_AdjustAngleAroundTarget(&delta, fieldSystem->camera);
    sTilted = TRUE;
}

void CameraTilt_Restore(FieldSystem *fieldSystem)
{
    if (sTilted && fieldSystem != NULL && fieldSystem->camera != NULL) {
        Camera_SetAngleAroundTarget(&sSavedAngle, fieldSystem->camera);
    }
    sTilted = FALSE;
}
