/*
 * Config.cpp
 *
 *  Created on: 2012-10-31
 *      Author: sam
 */


#include "Config.h"

using namespace Minisat;

#ifndef NDEBUG
int dbg_total_iterations=0;
#endif

static const char* _cat = "CORE";
static const char* _cat_sms = "SMS";
static const char* _cat_graph ="GRAPH";

 DoubleOption  Minisat::opt_var_decay         (_cat, "var-decay",   "The variable activity decay factor",            0.95,     DoubleRange(0, false, 1, false));
 DoubleOption  Minisat::opt_clause_decay      (_cat, "cla-decay",   "The clause activity decay factor",              0.999,    DoubleRange(0, false, 1, false));
 DoubleOption  Minisat::opt_random_var_freq   (_cat, "rnd-freq",    "The frequency with which the decision heuristic tries to choose a random variable", 0, DoubleRange(0, true, 1, true));
 DoubleOption  Minisat::opt_random_seed       (_cat, "rnd-seed",    "Used by the random variable selection",         91648253, DoubleRange(0, false, HUGE_VAL, false));
 IntOption     Minisat::opt_ccmin_mode        (_cat, "ccmin-mode",  "Controls conflict clause minimization (0=none, 1=basic, 2=deep)", 2, IntRange(0, 2));
 IntOption     Minisat::opt_phase_saving      (_cat, "phase-saving", "Controls the level of phase saving (0=none, 1=limited, 2=full)", 2, IntRange(0, 2));
 BoolOption    Minisat::opt_rnd_init_act      (_cat, "rnd-init",    "Randomize the initial activity", false);
 BoolOption    Minisat::opt_luby_restart      (_cat, "luby",        "Use the Luby restart sequence", true);
 IntOption     Minisat::opt_restart_first     (_cat, "rfirst",      "The base restart interval", 100, IntRange(1, INT32_MAX));
 DoubleOption  Minisat::opt_restart_inc       (_cat, "rinc",        "Restart interval increase factor", 2, DoubleRange(1, false, HUGE_VAL, false));
 DoubleOption  Minisat::opt_garbage_frac      (_cat, "gc-frac",     "The fraction of wasted memory allowed before a garbage collection is triggered",  0.20, DoubleRange(0, false, HUGE_VAL, false));


 BoolOption Minisat::opt_interpolate(_cat_sms,"interpolate","Store learnt interface clauses to form interpolants between modules",false);
 IntOption Minisat::opt_eager_prop(_cat_sms,"eager-prop","Controls whether unit propagation is allowed to cross subsolver boundaries. 0= Disable. 1= Enable. 2=Enable, but don't cross the last interpolant. 3= Enable, but don't cross the last interpolant, or any earlier solver. 4= Enable, but dont cross the last interpolant, or any earlier solver, unless they have already had their interpolants strengthened", 1, IntRange(0,5));
 IntOption Minisat::opt_subsearch(_cat_sms,"subsearch","Control how the solver performs search on the subsolvers: 0=abort as soon as a conflict backtracks past the supersolvers decisionlevel. 1=Abort only once a conflict on the super-interface variables is found, allowing backtracks past those variables in the process. 2=Abort only when the the super-solvers assignment is proven to be in conflict. 3=Don't continue subsearach if the subsolver has backtracked past super-solver decisions. 4=Don't continue past the last interpolant level if any solver has backtracked past a super solver's decisions",2,IntRange(0,4));

 BoolOption Minisat::opt_graph(_cat_graph,"graph","Use graph theory solver",true);
 BoolOption Minisat::opt_inc_graph(_cat_graph,"inc","Use incremental graph reachability",false);
 IntOption Minisat::opt_dec_graph(_cat_graph,"dec","Use decremental graph reachability",0,IntRange(0, 2));
 StringOption Minisat::opt_min_cut_alg(_cat_graph,"mincut","Select max-flow/min-cut algorithm (edmondskarp, edmondskarp, ibfs)","edmondskarp-adj");
 StringOption Minisat::opt_reach_alg(_cat_graph,"reach","Select reachability algorithm (connectivity, dijkstra)","connectivity");

  BoolOption Minisat::opt_conflict_shortest_path(_cat_graph,"conflict-shortest-path","Use shortest path (instead of arbitrary path) for conflict resolution",true);
  BoolOption Minisat::opt_conflict_min_cut(_cat_graph,"conflict-min-cut","Use min-cut (instead of arbitrary cut) for conflict resolution",false);
IntOption Minisat::opt_restrict_decisions(_cat_graph,"decisions","Restrict decisions to the first n variables (0 to disable)",0);

BoolOption Minisat::opt_check_solution(_cat_graph,"check-solution","Double check solution",true);
BoolOption Minisat::opt_print_reach(_cat_graph,"print-reach","Print reachability graphs",false);


BoolOption Minisat::opt_rnd_restart(_cat,"rnd-restart","Randomize activity on restart",false);

IntOption Minisat::opt_learn_reaches(_cat_graph,"learn-reach","Learn using reach variables: 0 = Never, 1=Paths, 2=Cuts,3=Always",0,IntRange(0,3));

StringOption Minisat::opt_priority(_cat_graph,"priority","Decision variable priority list","");

BoolOption Minisat::opt_print_conflicts(_cat,"print-conflicts","",false);

BoolOption Minisat::opt_rnd_phase(_cat,"rnd-phase","",false);
BoolOption Minisat::opt_init_rnd_phase(_cat,"init-rnd-phase","",false);

BoolOption Minisat::opt_reach_prop(_cat_graph,"prop-reach","",false);

BoolOption Minisat::opt_decide_graph(_cat_graph,"decide-graph","",false);
BoolOption Minisat::opt_use_random_path_for_decisions(_cat_graph,"decide-graph-rnd","",false);

BoolOption Minisat::opt_print_decision_path(_cat_graph,"decide-graph-print","",false);

MinCutAlg Minisat::mincutalg=ALG_EDMONSKARP ;
ReachAlg Minisat::reachalg=ALG_CONNECTIVITY;
ReachAlg Minisat::distalg=ALG_BFS;
