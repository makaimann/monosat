/****************************************************************************************[Solver.h]
The MIT License (MIT)

Copyright (c) 2014, Sam Bayless

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include "Config.h"

using namespace Monosat;

#ifndef NDEBUG
int dbg_total_iterations=0;
#endif

static const char* _cat = "CORE";
static const char* _cat_sms = "SMS";
static const char* _cat_graph ="GRAPH";
static const char* _cat_geom ="GEOMETRY";

 IntOption Monosat::opt_verb("MAIN","verb", "Verbosity level (0=silent, 1=some, 2=more).",1,IntRange(0,3));
 DoubleOption  Monosat::opt_var_decay         (_cat, "var-decay",   "The variable activity decay factor",            0.95,     DoubleRange(0, false, 1, false));
 DoubleOption  Monosat::opt_clause_decay      (_cat, "cla-decay",   "The clause activity decay factor",              0.999,    DoubleRange(0, false, 1, false));
 DoubleOption  Monosat::opt_random_var_freq   (_cat, "rnd-freq",    "The frequency with which the decision heuristic tries to choose a random variable", 0, DoubleRange(0, true, 1, true));
 DoubleOption  Monosat::opt_random_seed       (_cat, "rnd-seed",    "Used by the random variable selection",         91648253, DoubleRange(0, false, HUGE_VAL, false));
 IntOption     Monosat::opt_ccmin_mode        (_cat, "ccmin-mode",  "Controls conflict clause minimization (0=none, 1=basic, 2=deep)", 2, IntRange(0, 2));
 IntOption     Monosat::opt_phase_saving      (_cat, "phase-saving", "Controls the level of phase saving (0=none, 1=limited, 2=full)", 2, IntRange(0, 2));
 BoolOption    Monosat::opt_rnd_init_act      (_cat, "rnd-init",    "Randomize the initial activity", false);
 BoolOption    Monosat::opt_luby_restart      (_cat, "luby",        "Use the Luby restart sequence", true);
 IntOption     Monosat::opt_restart_first     (_cat, "rfirst",      "The base restart interval", 100, IntRange(1, INT32_MAX));
 DoubleOption  Monosat::opt_restart_inc       (_cat, "rinc",        "Restart interval increase factor", 2, DoubleRange(1, false, HUGE_VAL, false));
 DoubleOption  Monosat::opt_garbage_frac      (_cat, "gc-frac",     "The fraction of wasted memory allowed before a garbage collection is triggered",  0.20, DoubleRange(0, false, HUGE_VAL, false));

 IntOption Monosat::opt_time(_cat,"verb-time","Detail level of timing benchmarks (these add some overhead)",0,IntRange(0,5));

 DoubleOption  Monosat::opt_random_theory_freq   (_cat, "rnd-theory-freq",    "The frequency with which the decision theory solvers are selected to make decisions", 1, DoubleRange(0, true, 1, true));
 BoolOption  Monosat::opt_early_theory_prop   (_cat, "early-theory-prop",    "If false, the solver waits until all literals are propagated before propagating theories; if true, theories are propagated while the solver is still propagating literals", false);


 BoolOption Monosat::opt_interpolate(_cat_sms,"interpolate","Store learnt interface clauses to form interpolants between modules",false);
 IntOption Monosat::opt_eager_prop(_cat_sms,"eager-prop","Controls whether unit propagation is allowed to cross subsolver boundaries. 0= Disable. 1= Enable. 2=Enable, but don't cross the last interpolant. 3= Enable, but don't cross the last interpolant, or any earlier solver. 4= Enable, but dont cross the last interpolant, or any earlier solver, unless they have already had their interpolants strengthened", 1, IntRange(0,5));
 IntOption Monosat::opt_subsearch(_cat_sms,"subsearch","Control how the solver performs search on the subsolvers: 0=abort as soon as a conflict backtracks past the supersolvers decisionlevel. 1=Abort only once a conflict on the super-interface variables is found, allowing backtracks past those variables in the process. 2=Abort only when the the super-solvers assignment is proven to be in conflict. 3=Don't continue subsearach if the subsolver has backtracked past super-solver decisions. 4=Don't continue past the last interpolant level if any solver has backtracked past a super solver's decisions",2,IntRange(0,4));
 BoolOption Monosat::opt_permanent_theory_conflicts(_cat_sms,"permanent-theory-conflicts","True if conflict clauses from theory solvers are treated as permanent clauses; false if they are treated as learnt clauses (that is, allowed to be deleted by the solver)",false);
IntOption Monosat::opt_temporary_theory_conflicts(_cat_sms,"temporary-theory-conflicts","True if theory conflict clauses larger than this size should be discarded immediately.",INT32_MAX,IntRange(0, INT32_MAX));
IntOption Monosat::opt_temporary_theory_reasons(_cat_sms,"temporary-theory-reasons","True if theory reason clauses larger than this size should be discarded immediately.",INT32_MAX,IntRange(0, INT32_MAX));
 BoolOption Monosat::opt_graph(_cat_graph,"graph","Use graph theory solver",true);
 BoolOption Monosat::opt_inc_graph(_cat_graph,"inc","Use incremental graph reachability",false);
 IntOption Monosat::opt_dec_graph(_cat_graph,"dec","Use decremental graph reachability",0,IntRange(0, 2));
 StringOption Monosat::opt_min_cut_alg(_cat_graph,"maxflow","Select max s-t-flow algorithm (edmondskarp, edmondskarp-adj, edmondskarp-dynamic,dinitz,dinitz-linkcut)","edmondskarp-dynamic");//ibfs
 StringOption Monosat::opt_reach_alg(_cat_graph,"reach","Select reachability algorithm (bfs,dfs, dijkstra,ramal-reps,cnf)","ramal-reps");
 StringOption Monosat::opt_dist_alg(_cat_graph,"dist","Select reachability algorithm (bfs,dfs, dijkstra,ramal-reps,cnf)","ramal-reps");

 StringOption Monosat::opt_con_alg(_cat_graph,"connect","Select undirected reachability algorithm (bfs,dfs, dijkstra, thorup,cnf)","bfs");
StringOption Monosat::opt_undir_allpairs_alg(_cat_graph,"connect-allpairs","Select allpairs reachability algorithm (floyd-warshall,dijkstra, thorup)","floyd-warshall");
StringOption Monosat::opt_mst_alg(_cat_graph,"mst","Select minimum spanning tree algorithm (kruskal,prim,spira-pan)","spira-pan");

StringOption Monosat::opt_allpairs_alg(_cat_graph,"allpairs","Select allpairs reachability algorithm (floyd-warshall,dijkstra)","floyd-warshall");
StringOption Monosat::opt_components_alg(_cat_graph,"components","Select connected-components algorithm (disjoint-sets, link-cut)","disjoint-sets");
  BoolOption Monosat::opt_conflict_shortest_path(_cat_graph,"conflict-shortest-path","Use shortest path (instead of arbitrary path) for conflict resolution",true);
  BoolOption Monosat::opt_conflict_min_cut(_cat_graph,"conflict-min-cut","Use min-cut (instead of arbitrary cut) for conflict resolution",false);

BoolOption Monosat::opt_check_solution(_cat_graph,"check-solution","Double check solution",true);
BoolOption Monosat::opt_print_reach(_cat_graph,"print-reach","Print reachability graphs",false);
BoolOption Monosat::opt_print_graph(_cat_graph,"print-graph","Print digraph",false);

BoolOption Monosat::opt_compute_max_distance(_cat_graph,"max-distance","Optimize shortest path computation to abort when a path is longer than the longest path in the constraints.",true);
BoolOption Monosat::opt_learn_unreachable_component(_cat_graph,"learn-component","",false);
BoolOption Monosat::opt_force_directed(_cat_graph,"force-directed","Use directed reachability algorithms for undirected reachability queries (by duplicating edges as needed)\n",false);

BoolOption Monosat::opt_rnd_restart(_cat,"rnd-restart","Randomize activity on restart",false);

IntOption Monosat::opt_learn_reaches(_cat_graph,"learn-reach","Learn using reach variables: 0 = Never, 1=Paths, 2=Cuts,3=Always",0,IntRange(0,3));

StringOption Monosat::opt_priority(_cat_graph,"priority","Decision variable priority list","");

BoolOption Monosat::opt_print_conflicts(_cat,"print-conflicts","",false);

BoolOption Monosat::opt_rnd_phase(_cat,"rnd-phase","",false);
BoolOption Monosat::opt_init_rnd_phase(_cat,"init-rnd-phase","",false);

BoolOption Monosat::opt_encode_reach_underapprox_as_sat(_cat_graph,"reach-underapprox-cnf","Compute the under-approximate side of reachability constraints using CNF (only requires linear number of constraints), instead of the chosen algorithm",false);
BoolOption Monosat::opt_reach_prop(_cat_graph,"prop-reach","",false);

BoolOption Monosat::opt_decide_theories(_cat_graph,"decide-theories","",false);
BoolOption Monosat::opt_decide_graph_distance(_cat_graph,"decide-graph-dist","",false);

BoolOption Monosat::opt_use_random_path_for_decisions(_cat_graph,"decide-graph-rnd","",false);
BoolOption Monosat::opt_use_optimal_path_for_decisions(_cat_graph,"decide-graph-opt","When selecting a path during decisions, find the shortest path, excluding the weight of already assigned edges.",false);

DoubleOption Monosat::opt_decide_graph_re_rnd(_cat_graph,"decide-graph-re-rnd","Randomly make new random graphs for graph decisions instead of sticking with just one",0.01);

BoolOption Monosat::opt_print_decision_path(_cat_graph,"decide-graph-print","",false);
BoolOption Monosat::opt_force_distance_solver(_cat_graph,"force-distance","Force the graph distance solver to be used, instead of the optimized reachability solver",false);

DoubleOption Monosat::opt_allpairs_percentage(_cat_graph,"allpairs-frac","Fraction of nodes as source reach querries at which to trigger using allpairs solver instead of separate theory solvers. 0 to force all querries to go through allpairs solver. 1 to disable.",1,DoubleRange(0,true,1,true));

BoolOption Monosat::opt_decide_graph_neg(_cat_graph,"decide-graph-neg","",false);

BoolOption Monosat::opt_decide_graph_pos(_cat_graph,"decide-graph-pos","",true);

BoolOption Monosat::opt_ignore_theories(_cat_graph,"ignore-theories","",false);

//BoolOption Minisat::opt_check_pure_theory_lits(_cat_graph,"pure-theory-lits","",false);

BoolOption Monosat::opt_decide_graph_chokepoints(_cat_graph,"decide-graph-chokepoints","",false);
IntOption Monosat::opt_sort_graph_decisions(_cat_graph,"decide-graph-sort","0=dont sort, 1=sort by shortest, 2=sort by longest",0,IntRange(0,2));
BoolOption Monosat::opt_rnd_order_graph_decisions(_cat_graph,"decide-graph-rnd-order","",false);


BoolOption Monosat::opt_detect_pure_lits(_cat,"detect-pure-lits","Detect pure literals in the main solver (only during simplification, not during search)",false);

BoolOption Monosat::opt_detect_pure_theory_lits(_cat,"detect-pure-theory-lits","Detect pure literals in the theory solvers",true);

BoolOption Monosat::opt_mst_min_cut(_cat_graph,"mst-min-cut","Search for a min-cut during conflict resolution of disconnected minimum spanning trees",true);
BoolOption Monosat::opt_connected_components_min_cut(_cat_graph,"cc-mincut","Search for a min-cut during conflict resolution of connected components",true);
BoolOption Monosat::opt_optimize_mst(_cat_graph,"opt-mst","Find the solution that minimizes the spanning tree by making repeated sat checks.",false);
BoolOption Monosat::opt_skip_deletions(_cat_graph,"skip-deletions","",false);
BoolOption Monosat::opt_skip_additions(_cat_graph,"skip-additions","",false);

BoolOption Monosat::opt_allow_reach_decisions(_cat_graph,"allow-reach-decision","",true);

StringOption Monosat::opt_hull_alg(_cat_geom,"hull","Select convex-hull algorithm (monotone,quick)","monotone");


BoolOption Monosat::opt_conflict_dfs(_cat_graph,"conflict-dfs","Use a DFS (instead of a BFS) to find the conflict cut",true);
BoolOption Monosat::opt_conflict_from_source(_cat_graph,"conflict-from-source","Search from the source (instead of back from the target) to find the conflict cut",false);

ConvexHullAlg Monosat::hullAlg=ConvexHullAlg::ALG_MONOTONE_HULL;




BoolOption  Monosat::opt_propagate_theories_during_simplification(_cat,"theory-prop-during-simp","Apply propagation to theory solvers during simplification. Can be very expensive (depending on the theory).",true);
BoolOption Monosat::opt_shrink_theory_conflicts(_cat,"shrink-theory-conflicts","",false);

BoolOption Monosat::opt_rnd_shuffle(_cat_graph,"rnd-shuffle","Inject randomness into the solver by shuffling the order of propagation of graph constraints.",true);
BoolOption Monosat::opt_components_learn_connect(_cat_graph, "components-learn-connect","",false);

BoolOption Monosat::opt_dinics_recursive(_cat_graph,"dinitz-recursive","Use the recursive (default: iterative) Dinic's Maximum-flow implementation",false);

IntOption     Monosat::opt_width("GRAPH","width","Width of graph.\n", 0, IntRange(0, INT32_MAX));
    IntOption     Monosat::opt_height("GRAPH","height","Height of graph.\n", 0, IntRange(0, INT32_MAX));
    IntOption     Monosat::opt_bits("GRAPH","bits","Bits per position in graph.\n", 1, IntRange(0, INT32_MAX));

    BoolOption	  Monosat::opt_csv("GRAPH","csv","Output in CSV format",false);

MinCutAlg Monosat::mincutalg=MinCutAlg::ALG_EDMONSKARP ;
ReachAlg Monosat::reachalg=ReachAlg::ALG_RAMAL_REPS;
ConnectivityAlg Monosat::undirectedalg=ConnectivityAlg::ALG_DFS;
DistAlg Monosat::distalg=DistAlg::ALG_RAMAL_REPS;
AllPairsAlg Monosat::allpairsalg=AllPairsAlg::ALG_DIJKSTRA_ALLPAIRS;
AllPairsConnectivityAlg Monosat::undirected_allpairsalg=AllPairsConnectivityAlg::ALG_DIJKSTRA_ALLPAIRS;
ComponentsAlg Monosat::componentsalg =ComponentsAlg::ALG_DISJOINT_SETS;
MinSpanAlg Monosat::mstalg = MinSpanAlg::ALG_KRUSKAL;

PointInPolygonAlg Monosat::pipalg = PointInPolygonAlg::ALG_RECURSIVE_SPLIT;
