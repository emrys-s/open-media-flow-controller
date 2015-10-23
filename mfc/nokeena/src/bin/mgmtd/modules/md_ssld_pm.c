/*
 *
 * Filename:  md_ssld_pm.c
 * Date:      24 May 2011
 * Author:    Kiran Desai
 *
 * (C) Copyright 2011 Juniper Networks, Inc.
 * All rights reserved.
 *
 *
 */

/* ------------------------------------------------------------------------- */
/* md_ssld_pm.c: shows how to add new instances of wildcard nodes to
 * the initial database, and how to override defaults for nodes
 * created by Samara base components.  Also shows how to add a process
 * of yours to be launched and monitored by Process Manager.
 *
 * This module does not register any nodes of its own: it's sole
 * purpose is to add/change nodes in the initial database.  But there
 * would be no problem if we did want to register nodes here.
 */

#include "common.h"
#include "dso.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "nkn_mgmt_common.h"

#define	SSLD_PATH	 "/opt/nkn/sbin/ssld"
#define SSLD_WORKING_DIR "/coredump/ssld"
#define SSLD_CONFIG_FILE "/config/nkn/nkn.conf.default"

int md_ssld_pm_init(const lc_dso_info *info, void *data);
static md_upgrade_rule_array *md_ssld_pm_ug_rules = NULL;

static int
md_ssld_pm_upgrade_downgrade(const mdb_db *old_db,
                          mdb_db *inout_new_db,
                          uint32 from_module_version,
                          uint32 to_module_version, void *arg);
/* ------------------------------------------------------------------------- */
/* In initial values, we are not required to specify values for nodes
 * underneath wildcards for which we want to accept the default.
 * Here we are just specifying the ones where the default is not what
 * we want, or where we don't want to rely on the default not changing.
 */
const bn_str_value md_ssld_pm_initial_values[] = {
    /* .....................................................................
     * ssld daemon: basic launch configuration.
     */
    {"/pm/process/ssld", bt_string, "ssld"},
    {"/pm/process/ssld/launch_enabled", bt_bool, "true"},
    {"/pm/process/ssld/auto_launch", bt_bool, "true"},
    {"/pm/process/ssld/auto_relaunch", bt_bool, "true"},
    {"/pm/process/ssld/delete_trespassers", bt_bool, "true"},
    {"/pm/process/ssld/working_dir", bt_string, SSLD_WORKING_DIR},
    {"/pm/process/ssld/launch_path", bt_string, SSLD_PATH},
    {"/pm/process/ssld/launch_uid", bt_uint16, "0"},
    {"/pm/process/ssld/launch_gid", bt_uint16, "0"},
    {"/pm/process/ssld/kill_timeout", bt_duration_ms, "12000"},
    {"/pm/process/ssld/term_signals/force/1", bt_string, "SIGKILL"},
    {"/pm/process/ssld/term_signals/normal/1", bt_string, "SIGKILL"},
    {"/pm/process/ssld/launch_params/1/param", bt_string, "-f"},
    {"/pm/process/ssld/launch_params/2/param", bt_string, SSLD_CONFIG_FILE},
    {"/pm/process/ssld/launch_params/3/param", bt_string, "-D"},
    {"/pm/process/ssld/environment/set/PYTHONPATH", bt_string, "PYTHONPATH"},
    {"/pm/process/ssld/environment/set/PYTHONPATH/value", bt_string, "${PYTHONPATH}:/nkn/plugins"},
    {"/pm/process/ssld/initial_launch_order", bt_int32, NKN_LAUNCH_ORDER_ADNSD},
    {"/pm/process/ssld/initial_launch_timeout", bt_duration_ms, NKN_LAUNCH_TIMEOUT_ADNSD},
    {"/pm/process/ssld/description", bt_string, "SSLD"},
    {"/pm/process/ssld/environment/set/LD_LIBRARY_PATH", bt_string, "LD_LIBRARY_PATH"},
    {"/pm/process/ssld/environment/set/LD_LIBRARY_PATH/value", bt_string, "/opt/nkn/lib:/lib/nkn"},

    /* Set additional library paths that need to be saved in case of a core dump */
    {"/pm/process/ssld/save_paths/1", bt_uint8, "1"},
    {"/pm/process/ssld/save_paths/1/path", bt_string, "/lib/nkn"},
    {"/pm/process/ssld/save_paths/2", bt_uint8, "2"},
    {"/pm/process/ssld/save_paths/2/path", bt_string, "/opt/nkn/lib"},
    /*Adding nodes that got deleted in Denali*/
    {"/pm/process/ssld/expected_exit_backoff", bt_bool, "false"},
    {"/pm/process/ssld/expected_exit_code", bt_int16, "-1"},
    {"/pm/process/ssld/fd_limit", bt_uint32, "0"},
    {"/pm/process/ssld/launch_params/1", bt_uint8, "1"},
    {"/pm/process/ssld/launch_params/2", bt_uint8, "2"},
    {"/pm/process/ssld/launch_params/3", bt_uint8, "3"},
    {"/pm/process/ssld/launch_pre_delete", bt_string, ""},
    {"/pm/process/ssld/launch_priority", bt_int32, "0"},
    {"/pm/process/ssld/liveness/data", bt_string, ""},
    {"/pm/process/ssld/liveness/enable", bt_bool, "false"},
    {"/pm/process/ssld/liveness/hung_count", bt_uint32, "4"},
    {"/pm/process/ssld/liveness/type", bt_string, "gcl_client"},
    {"/pm/process/ssld/log_output", bt_string, "both"},
    {"/pm/process/ssld/max_snapshots", bt_uint16, "10"},
    {"/pm/process/ssld/memory_size_limit", bt_int64, "-1"},
    {"/pm/process/ssld/reexec_path", bt_string, ""},
    {"/pm/process/ssld/restart_action", bt_string, "restart_self"},
    {"/pm/process/ssld/shutdown_order", bt_int32, "0"},
    {"/pm/process/ssld/shutdown_pre_delay", bt_duration_ms, "0"},
    {"/pm/process/ssld/term_action", bt_name, ""},
    {"/pm/process/ssld/term_signals/force/2", bt_string, "SIGQUIT"},
    {"/pm/process/ssld/term_signals/force/3", bt_string, "SIGKILL"},
    {"/pm/process/ssld/term_signals/liveness/1", bt_string, "SIGTERM"},
    {"/pm/process/ssld/term_signals/liveness/2", bt_string, "SIGQUIT"},
    {"/pm/process/ssld/term_signals/liveness/3", bt_string, "SIGKILL"},
    {"/pm/process/ssld/term_signals/normal/2", bt_string, "SIGQUIT"},
    {"/pm/process/ssld/term_signals/normal/3", bt_string, "SIGKILL"},
    {"/pm/process/ssld/terminate_on_shutdown", bt_bool, "true"},
    {"/pm/process/ssld/tz_change/action", bt_string, "none"},
    {"/pm/process/ssld/tz_change/signal", bt_string, ""},
    {"/pm/process/ssld/umask", bt_uint32, "0"},
    {"/pm/process/ssld/working_dir", bt_string, "/coredump/ssld"},

};


/* ------------------------------------------------------------------------- */
static int
md_ssld_pm_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    err = mdb_create_node_bindings
        (commit, db, md_ssld_pm_initial_values,
         sizeof(md_ssld_pm_initial_values) / sizeof(bn_str_value));
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
int
md_ssld_pm_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Commit order of 200 is greater than the 0 used by most modules,
     * including md_pm.c.  This is to ensure that we can override some
     * of PM's global configuration, such as liveness grace period.
     *
     * We need modrf_namespace_unrestricted to allow us to set nodes
     * from our initial values function that are not underneath our
     * module root.
     */
    err = mdm_add_module
	("ssld_pm", 7, "/pm/nkn/ssld", NULL,
	 modrf_namespace_unrestricted,
	 NULL, NULL, NULL, NULL, NULL, NULL, 300, 0,
	 md_ssld_pm_upgrade_downgrade,
	 &md_ssld_pm_ug_rules,
	 md_ssld_pm_add_initial, NULL,
	 NULL, NULL, NULL, NULL, &module);
    bail_error(err);

    /* Upgrade processing nodes */
    err = md_upgrade_rule_array_new(&md_ssld_pm_ug_rules);
    bail_error(err);
    ra = md_ssld_pm_ug_rules;

    MD_NEW_RULE(ra, 0, 1);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/ssld/launch_enabled";
    rule->mur_new_value_str =   "false";
    rule->mur_new_value_type =  bt_bool;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 0, 1);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/pm/process/ssld/auto_launch";
    rule->mur_new_value_str =   "false";
    rule->mur_new_value_type =  bt_bool;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/auto_relaunch";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "true";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/delete_trespassers";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "true";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/expected_exit_backoff";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/expected_exit_code";
    rule->mur_new_value_type =  bt_int16;
    rule->mur_new_value_str =   "-1";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/fd_limit";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/launch_params/1";
    rule->mur_new_value_type =  bt_uint8;
    rule->mur_new_value_str =   "1";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/launch_params/2";
    rule->mur_new_value_type =  bt_uint8;
    rule->mur_new_value_str =   "2";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/launch_params/3";
    rule->mur_new_value_type =  bt_uint8;
    rule->mur_new_value_str =   "3";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/launch_pre_delete";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/launch_priority";
    rule->mur_new_value_type =  bt_int32;
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/liveness/data";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/liveness/enable";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/liveness/hung_count";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "4";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/liveness/type";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "gcl_client";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/log_output";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "both";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/max_snapshots";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "10";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/memory_size_limit";
    rule->mur_new_value_type =  bt_int64;
    rule->mur_new_value_str =   "-1";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/restart_action";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "restart_self";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/shutdown_order";
    rule->mur_new_value_type =  bt_int32;
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/shutdown_pre_delay";
    rule->mur_new_value_type =  bt_duration_ms;
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/term_action";
    rule->mur_new_value_type =  bt_name;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/term_signals/force/2";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "SIGQUIT";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/term_signals/force/3";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "SIGKILL";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/term_signals/liveness/1";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "SIGTERM";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/term_signals/liveness/2";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "SIGQUIT";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/term_signals/liveness/3";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "SIGKILL";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/term_signals/normal/2";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "SIGQUIT";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/term_signals/normal/3";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "SIGKILL";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/terminate_on_shutdown";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "true";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/tz_change/action";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "none";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/tz_change/signal";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/working_dir";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "/coredump/ssld";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/umask";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/pm/process/ssld/reexec_path";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

bail:
    return(err);
}


static int
md_ssld_pm_upgrade_downgrade(const mdb_db *old_db,
		mdb_db *inout_new_db,
		uint32 from_module_version,
		uint32 to_module_version, void *arg)
{
    int err = 0;

    err = md_generic_upgrade_downgrade(old_db, inout_new_db, from_module_version,
	    to_module_version, arg);
    bail_error(err);
    /*
     *  New upgrade rule to re-initialize the ssld pm nodes for 11.A.2
     */
    if ((to_module_version == 3)) {
	err = mdb_create_node_bindings(NULL, inout_new_db,
		md_ssld_pm_initial_values,
		sizeof( md_ssld_pm_initial_values )/sizeof(bn_str_value));
	bail_error(err);

    }
    else if ((to_module_version == 4)) {
	err = mdb_create_node_bindings(NULL, inout_new_db,
		md_ssld_pm_initial_values,
		sizeof( md_ssld_pm_initial_values )/sizeof(bn_str_value));
	bail_error(err);
    }
bail:
    return err;
}
