/* 
 * Dynamic sync control driver definitions (V2)
 * 
 * by andip71 (alias Lord Boeffla)
 * 
 */

#define DYN_FSYNC_ACTIVE_DEFAULT true
#define DYN_FSYNC_VERSION_MAJOR 2
#define DYN_FSYNC_VERSION_MINOR 0

extern bool dyn_sync_scr_suspended;
extern bool dyn_fsync_active;
extern void dyn_fsync_suspend_actions(void);
