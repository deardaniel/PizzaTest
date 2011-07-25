
#ifndef _CMUUSKAL16_H
#define _CMUUSKAL16_H

#ifdef __cplusplus
extern "C" {
#endif

#define VOXNAME cmu_us_kal16
#define VOXHUMAN "Kevin"
#define VOXGENDER "male"
#define VOXVERSION 1.1

cst_voice *register_cmu_us_kal16(const char *voxdir);
cst_voice *unregister_cmu_us_kal16(cst_voice *vox);

#ifdef __cplusplus
};
#endif

#endif /* _CMUUSKAL16_H */




