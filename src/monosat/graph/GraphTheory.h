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

#ifndef GRAPH_THEORY_H_
#define GRAPH_THEORY_H_

#include "monosat/utils/System.h"
#include "monosat/core/Theory.h"
#include "monosat/core/Config.h"
#include "monosat/dgl/Reach.h"
#include "monosat/dgl/Dijkstra.h"
#include "monosat/dgl/BFS.h"

#include "monosat/core/SolverTypes.h"
#include "monosat/mtl/Map.h"
#include "monosat/dgl/MaxFlow.h"
#include "monosat/dgl/DynamicGraph.h"
#include "monosat/dgl/EdmondsKarp.h"
#include "monosat/dgl/EdmondsKarpAdj.h"
#include "monosat/dgl/EdmondsKarpDynamic.h"
#include "monosat/dgl/KohliTorr.h"
#include "monosat/dgl/Dinics.h"
#include "monosat/dgl/DinicsLinkCut.h"

#include "monosat/dgl/Chokepoint.h"
#include "monosat/graph/WeightedDijkstra.h"
#include "monosat/graph/GraphTheoryTypes.h"
#include "monosat/utils/System.h"
#include "monosat/core/Solver.h"

#include "monosat/graph/AllPairsDetector.h"
#include "monosat/graph/ReachDetector.h"
#include "monosat/bv/BVTheorySolver.h"

#include "monosat/graph/DistanceDetector.h"
#include "monosat/graph/WeightedDistanceDetector.h"
#include "monosat/graph/MSTDetector.h"
#include "monosat/graph/MaxflowDetector.h"
#include "monosat/graph/ConnectedComponentsDetector.h"
#include "monosat/graph/CycleDetector.h"
#include "monosat/graph/SteinerDetector.h"
#include <vector>
#include <gmpxx.h>
#include <cstdio>
#include <cstdlib>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sstream>
#include <string>
#include <monosat/amo/AMOTheory.h>

using namespace dgl;
namespace Monosat {


template<typename Weight>
class GraphTheorySolver: public Theory, public TheorySolver, public BVTheory{
public:

	double rnd_seed;

private:
	Solver * S;
	int local_q = 0;
	bool lazy_backtracking_enabled=false;
	vec<Theory*> propagation_required_theories;
public:
	int n_satisfied_detectors=0;
	bool all_edges_unit = true;
	bool all_edges_positive=true;
	bool has_any_bitvector_edges=false;
	int lazy_backtrack_level=-1;
	vec<lbool> assigns;
	MSTDetector<Weight> * mstDetector = nullptr;
	struct ReachabilityConstraint {
		int from;
		int to;
		int distance;
		Var reach_var;
	};

	vec<ReachabilityConstraint> unimplemented_reachability_constraints;
	struct DistanceConstraint {
		int from;
		int to;
		Weight distance;
		Var reach_var;
		bool strict;
	};
	vec<DistanceConstraint> unimplemented_distance_constraints;

	struct DistanceConstraintBV {
			int from;
			int to;
			int bvID;
			Var var;
			bool strict;
		};
	vec<DistanceConstraintBV> unimplemented_distance_constraints_bv;

	struct MaxflowConstraintBV {
			int s;
			int t;
			int bvID;
			Var var;
			bool strict;
		};
	vec<MaxflowConstraintBV> unimplemented_maxflow_constraints_bv;

	CRef graph_decision_reason = CRef_Undef;

	DynamicGraph<Weight> g_under;
	DynamicGraph<Weight> g_over;
	bool using_neg_weights = false;
	DynamicGraph<Weight> g_under_weights_over;
	DynamicGraph<Weight> g_over_weights_under;

	/**
	 * The cutgraph is (optionally) used for conflict analysis by some graph theories.
	 * It has two edges for every edge in the real graph (with indices edgeID*2 and edgeID*2+1).
	 * If edge ID is assigned to FALSE, then edge ID*2 is assigned to enabled in the cutgraph, and
	 * edge ID*2+1 is disabled.
	 * Otherwise, if edge ID is unassigned or true, then edge ID*2 is disabled in the cutgraph, and
	 * edge ID*2+1 is enabled.
	 */
	DynamicGraph<int64_t> cutGraph;

	/*struct ComparisonStatus{//:public BVTheorySolver<int64_t>::CallBack{
		GraphTheorySolver & outer;

		void comparisonAltered(int bvID, int comparisonID){

		}
		void operator()(int bvID){
			if(!outer.bv_needs_update[bvID]){
				outer.bv_needs_update[bvID]=true;
				outer.bvs_to_update.push(bvID);
			}
			int edgeID = outer.getBVEdge(bvID);
			outer.g_under.setEdgeWeight(edgeID,outer.edge_bv_weights[bvID].getUnder());
			outer.g_over.setEdgeWeight(edgeID, outer.edge_bv_weights[bvID].getOver());
			if(outer.using_neg_weights){
				outer.g_under_weights_over.setEdgeWeight(edgeID,outer.edge_bv_weights[bvID].getOver());
				outer.g_over_weights_under.setEdgeWeight(edgeID, outer.edge_bv_weights[bvID].getUnder());
			}
		}
		ComparisonStatus(GraphTheorySolver & outer):outer(outer){}
	} bvcallback;*/

	//typedef BVTheorySolver<Weight,ComparisonStatus>::BitVector BitVector;
	//if bitvectors weights are supplied, then this manages the resulting weights.
	BVTheorySolver<Weight> * bvTheory=nullptr;

	vec<const char*> node_symbols;
	vec<const char*> edge_symbols;

	struct Trail{
		//Lit l=lit_Undef;
		int level = -1;
		Var prev_var=var_Undef;
		Var next_var=var_Undef;
	};
	vec<Trail> trail;//we can size this in advance, to the size of the variables;
	vec<Var> decisions;//have the prev_var of the decision point to the end of the current level's trail, if any.
	Var lazy_trail_head = var_Undef;//only used when lazy backtracking; this is the tail of the trail matching the SAT solver's trail.
	Var getPrev(Var v){
		return trail[v].prev_var;
		/*int lev = trail[v].level;
		if(lev>=0){
			Var dec = decisions[lev];
			if(dec==v){
				//return getBack(lev-1);
				return var_Undef;
			}else{
				return trail[v].prev_var;
			}
		}else if (lev==-2){

		}
		return var_Undef;*/
	}
	Var inline _getPrev(Var v){

		return trail[v].prev_var;
	}
	Var inline _getNext(Var v){

		return trail[v].next_var;
	}
	Var getNext(Var v){
		return trail[v].next_var;
	/*	int lev = trail[v].level;
		if(lev>=0){
			Var dec = decisions[lev];
			if(trail[v].next_var==dec){
				//return getDecision(lev+1);
				return var_Undef;
			}else{
				return trail[v].next_var;
			}

		}
		return var_Undef;*/
	}
	Var getDecision(int lev){
		if(lev>=0){
			//there is no level 0 decision, but there is a level 0 head of the tail, which will operate the same way.
			return decisions[lev];
		}else{
			return var_Undef;
		}
	}
	Var getBack(int lev){
		assert(lev>=0);
		//if(lev>=0){
		if(decisions[lev]==var_Undef){
			return var_Undef;
		}else{
			return trail[decisions[lev]].prev_var;
		}
	/*	}else{
			return var_Undef;
		}*/
	}
	bool onLazyTrail(Var v){
		return trail[v].level==-2;
	}
	bool onTrail(Var v){
		return trail[v].level!=-1;
	}
	//move a literal from the 'real' trail (which is in sync with the SAT solver) to the lazy trail (if lazy backtracking is enabled).
	void prependToLazyTrail(Lit l){
		//dbg_check_trails();
		removeFromTrail(var(l));

		Var v = var(l);
		assert(trail[v].level==-1);

		Var dec = lazy_trail_head;
		if(dec==var_Undef){
			lazy_trail_head=v;
			assert(trail[v].level==-1);
			assert(trail[v].prev_var==var_Undef);
			assert(trail[v].next_var==var_Undef);
			trail[v].level=-2;
			trail[v].prev_var=v;
			trail[v].next_var=v;//the decision is a self-loop
		}else{
			Var p = trail[dec].prev_var;
			trail[dec].prev_var=v;
			trail[v].next_var=dec;
			trail[p].next_var=v;
			trail[v].prev_var=p;
			trail[v].level =-2;
			lazy_trail_head=v;
		}

		//dbg_check_trails();
	}

	//move a literal from the 'real' trail (which is in sync with the SAT solver) to the lazy trail (if lazy backtracking is enabled).
	void appendToLazyTrail(Lit l){
		//dbg_check_trails();
		removeFromTrail(var(l));

		Var v = var(l);
		assert(trail[v].level==-1);

		Var dec = lazy_trail_head;
		if(dec==var_Undef){
			lazy_trail_head=v;
			assert(trail[v].level==-1);
			assert(trail[v].prev_var==var_Undef);
			assert(trail[v].next_var==var_Undef);
			trail[v].level=-2;
			trail[v].prev_var=v;
			trail[v].next_var=v;//the decision is a self-loop
		}else{
			Var back = trail[dec].prev_var;
			trail[back].next_var=v;
			trail[dec].prev_var=v;
			trail[v].next_var=dec;
			trail[v].prev_var=back;
			trail[v].level =-2;
		}
		//dbg_check_trails();
	}

	void insertIntoTrail(Lit l, Var insertAfter){
		//dbg_check_trails();
		assert(insertAfter!=var_Undef);
		Var v = var(l);
		assert(trail[v].level==-1);
		Var n = trail[insertAfter].next_var;
		trail[insertAfter].next_var=v;
		trail[n].prev_var=v;
		trail[v].next_var=n;
		trail[v].prev_var=insertAfter;
		trail[v].level =trail[insertAfter].level;
		//dbg_check_trails();
	}
	void appendToTrail(Lit l, int lev){
		//dbg_check_trails();
		Var v = var(l);
		assert(trail[v].level==-1);
		while(lev>=decisions.size()){
			decisions.push(var_Undef);
		}
		Var dec = decisions[lev];
		if(dec==var_Undef){
			decisions[lev]=v;
			assert(trail[v].level==-1);
			assert(trail[v].prev_var==var_Undef);
			assert(trail[v].next_var==var_Undef);
			trail[v].level=lev;
			trail[v].prev_var=v;
			trail[v].next_var=v;//the decision is a self-loop
		}else{
			Var back = trail[dec].prev_var;
			trail[v].level =lev;
			trail[back].next_var=v;
			trail[dec].prev_var=v;
			trail[v].next_var=dec;
			trail[v].prev_var=back;
		}
		//dbg_check_trails();
	}
	bool removeFromTrail(Var v){
		//dbg_check_trails();
		int lev =trail[v].level;
		if(lev>=0 || lev==-2){
			while(bvtrail.size() && bvtrail.last().graphAssign==v){
				bvtrail.pop();
			}

			Var prev = trail[v].prev_var;
			Var next = trail[v].next_var;
			assert(prev!=var_Undef);
			assert(next!=var_Undef);
			trail[prev].next_var=next;
			trail[next].prev_var=prev;

			trail[v].next_var=var_Undef;
			trail[v].prev_var=var_Undef;
			trail[v].level=-1;

			if(lev >=0 && v== decisions[lev]){
				if(next==v){
					//v is the last element of this level
					assert(prev==v);
					decisions[lev]=var_Undef;
					dbg_check_trails();
					return true;
				}else{
					decisions[lev]=next;
				}
			}else if (lev ==-2 && v== lazy_trail_head){
				if(next==v){
					//v is the last element of this level
					assert(prev==v);
					lazy_trail_head=var_Undef;
					dbg_check_trails();
					return true;
				}else{
					lazy_trail_head=next;
				}
			}

		}
		//dbg_check_trails();
		return false;
	}
	//Returns true if the trail at this level is now empty.
	inline bool _removeFromTrail(Var v, int lev){
			while(bvtrail.size() && bvtrail.last().graphAssign==v){
				bvtrail.pop();
			}

			assert(lev>=0);
			Var prev = trail[v].prev_var;
			Var next = trail[v].next_var;
			assert(prev!=var_Undef);
			assert(next!=var_Undef);
			trail[prev].next_var=next;
			trail[next].prev_var=prev;

			trail[v].next_var=var_Undef;
			trail[v].prev_var=var_Undef;
			trail[v].level=-1;

			if(v== decisions[lev]){
				if(next==v){
					//v is the last element of this level
					assert(prev==v);
					decisions[lev]=var_Undef;
					dbg_check_trails();
					return true;
				}else{
					decisions[lev]=next;
				}
			}
			//dbg_check_trails();
			return false;
		}
	void dbg_check_trail(int lev){
#ifdef DEBUG_GRAPH
		static vec<bool> seen;
		seen.clear();
		seen.growTo(vars.size());
		if(lev>=0){
			Var decision = decisions[lev];
			if(decision!=var_Undef){
				seen[decision]=true;
				assert(trail[decision].level==lev);
				Var v = trail[decision].next_var;
				if(v==decision){
					assert(trail[v].prev_var==v);//this is a self loop.
				}
				Var p = decision;
				while(v!=decision){
					assert(!seen[v]);
					seen[v]=true;
					assert(trail[v].level==lev);
					assert(trail[v].prev_var==p);
					p=v;
					v=trail[v].next_var;
				}
			}
		}else if (lev==-2){

			Var decision = lazy_trail_head;
			if(decision!=var_Undef){
				assert(opt_lazy_backtrack && supportsLazyBacktracking());
				seen[decision]=true;
				assert(trail[decision].level==lev);
				Var v = trail[decision].next_var;
				if(v==decision){
					assert(trail[v].prev_var==v);//this is a self loop.
				}
				Var p = decision;
				while(v!=decision){
					assert(!seen[v]);
					seen[v]=true;
					assert(trail[v].level==lev);
					assert(trail[v].prev_var==p);
					p=v;
					v=trail[v].next_var;
				}
			}
		}
#endif
	}

	void dbg_check_trails(){
/*#ifdef DEBUG_GRAPH

		int last_pos=-1;
		for(int i = 0;i<bvtrail.size();i++){
			Var v = bvtrail[i].graphAssign;
			int bvpos = bvtrail[i].bvTrail;
			if(v!=var_Undef)
				assert(value(v)!=l_Undef);
			assert(bvpos>=last_pos);
			last_pos=bvpos;
			assert(bvpos<bvTheory->trail.size());
		}

		dbg_check_trail(-2);//lazy trail
		for(int l = 0;l<decisions.size();l++){
			dbg_check_trail(l);
		}
		for(int v = 0;v<nVars();v++){
			int l = trail[v].level;
			if(l>=0){
				assert(l<decisions.size());
				assert(decisions[l]!=var_Undef);
			}else if (l==-2){
				assert(opt_lazy_backtrack && supportsLazyBacktracking());
				assert(lazy_trail_head!=var_Undef);
			}
		}
#endif*/
	}

	//vec<Assignment> trail;
	//vec<int> trail_lim;


	struct ReachInfo {
		int source;
		bool distance = 0;
		bool weighted_distance=0;
		Detector * detector;

		ReachInfo() :
			source(-1), detector(nullptr) {
		}
	};
	vec<ReachInfo> weighted_dist_info;
	vec<ReachInfo> dist_info;
	vec<ReachInfo> reach_info;
	vec<ReachInfo> connect_info;
public:
	vec<Theory*> theories;
	vec<bool> satisfied_detectors;
	vec<Detector*> detectors;
	vec<ReachDetector<Weight>*> reach_detectors;
	vec<DistanceDetector<Weight>*> distance_detectors;
	vec<WeightedDistanceDetector<Weight>*> weighted_distance_detectors;
	vec<MaxflowDetector<Weight>*> flow_detectors;
	ConnectedComponentsDetector<Weight>* component_detector = nullptr;
	CycleDetector<Weight> * cycle_detector = nullptr;
	vec<SteinerDetector<Weight>*> steiner_detectors;

	struct MarkerEntry{
		int id=-1;
		bool forTheory=false;
        bool forHeuristic=false;
	};
	vec<MarkerEntry> marker_map;

	std::vector<MaxFlowEdge> cut;

	//Full matrix
	//vec<vec<Edge> > edges;
	
	//Just a list of the edges
	vec<Edge> edge_list;
	vec<vec<Edge> > undirected_adj;
	vec<vec<Edge> > inv_adj;

	//vector of the weights for each edge
	std::vector<Weight> edge_weights;
	std::vector<BitVector<Weight>> edge_bv_weights;
	struct ComparisonID{
		Weight w;
		Lit l;
		int bvID:31;
		int is_lt:1;
	};
	vec<ComparisonID> comparisons;
	vec<vec<int> > comparisons_lt;
	vec<vec<int> > comparisons_gt;
	vec<vec<int> > comparisons_leq;
	vec<vec<int> > comparisons_geq;


	struct BVInfo{
		int edgeID=-1;
		int detectorID=-1;
	};


	vec<BVInfo> bitvector_data;
/*
	vec<bool> bv_needs_update;
	vec<int> bvs_to_update;
*/

	bool requiresPropagation = true;
	//int n_decisions = 0;
	//vec<int> var_decision_pos;
	int n_theory_solves=0;
	vec<char> seen;
	vec<int> to_visit;

	vec<Lit> tmp_clause;
	//Data about local theory variables, and how they connect to the sat solver's variables
	struct VarData {
		int isEdge :1;
		int isBV:1;
		int isTheoryVar:1;
		int isSatisfied:1;
		//int occursPositive :1;
		//int occursNegative :1;
		int detector_edge :26;	//the detector this variable belongs to, or its edge number, if it is an edge variable

		Var solverVar;
		Var theory_var;
	};

	vec<VarData> vars;

	struct SatisfiedLit{
		int level=-1;
		Lit l=lit_Undef;
		SatisfiedLit(int level, Lit l):level(level),l(l){

		}
	};
	vec<SatisfiedLit> satisfied_lits;

	int theory_index = 0;
public:
	
	double mctime = 0;
	double reachtime = 0;
	double unreachtime = 0;
	double pathtime = 0;
	double propagationtime = 0;
	int64_t propagations =-1;
	int64_t stats_propagations = 0;
	int64_t stats_num_conflicts = 0;
	int64_t stats_num_skipped_edgeset_props=0;
	int64_t stats_num_lazy_conflicts=0;
	int64_t stats_decisions = 0;
	int64_t stats_num_reasons = 0;
	long stats_bv_backtracks =0;
	long stats_bv_enqueues =0;
	long stats_bv_enqueue_while_sat=0;
	long stats_backtrack_assigns = 0;
	long stats_enqueues=0;
	double reachupdatetime = 0;
	double unreachupdatetime = 0;
	double stats_initial_propagation_time = 0;
	double stats_decision_time = 0;
	double stats_reason_initial_time = 0;
	double stats_reason_time = 0;
	int64_t num_learnt_paths = 0;
	int64_t learnt_path_clause_length = 0;
	int64_t num_learnt_cuts = 0;
	int64_t learnt_cut_clause_length = 0;
	int64_t stats_pure_skipped = 0;
	int64_t stats_mc_calls = 0;
	int64_t stats_propagations_skipped = 0;

	int64_t stats_lazy_decisions = 0;
	vec<Lit> reach_cut;

	struct CutStatus {
		int one = 1;
		int inf = 0xFFFF;
		GraphTheorySolver & outer;

		const int &operator [](int id) const {
			return one;
			/*if(outer.value(outer.edge_list[id].v) ==l_False){
			 return one;
			 }else{
			 return inf;
			 }*/
		}
		int size() const {
			return outer.edge_list.size();
		}
		CutStatus(GraphTheorySolver & _outer) :
				outer(_outer) {
		}
		
	} cutStatus;

	struct PropCutStatus {
		GraphTheorySolver & outer;
		int operator ()(int id) const {
			
			if (outer.value(outer.edge_list[id].v) == l_Undef) {
				return 1;
			} else {
				assert(outer.value(outer.edge_list[id].v)==l_True);
				return 0xF0F0F0;
			}
		}
		PropCutStatus(GraphTheorySolver & _outer) :
				outer(_outer) {
		}
		
	} propCutStatus;
    vec<Theory*> & getTheories(){
        return theories;
    }
	GraphTheorySolver(Solver * S_) :
			S(S_), cutStatus(*this), propCutStatus(*this){

		if(opt_record){
			std::string t = (const char*)opt_record_file;
			t+="/LOG_GRAPH_UNDER" +std::to_string(S->getTheories().size());
			g_under.outfile = fopen(t.c_str(), "w");
		}
		if(opt_record){
			std::string t = (const char*)opt_record_file;
			t+="/LOG_GRAPH_OVER" +std::to_string(S->getTheories().size());
			g_over.outfile = fopen(t.c_str(), "w");
		}
		if(opt_record){
			std::string t = (const char*)opt_record_file;
			t+="/LOG_GRAPH_CUT" +std::to_string(S->getTheories().size());
			cutGraph.outfile = fopen(t.c_str(), "w");
		}

		g_under.disable_history_clears=opt_disable_history_clears;
		g_over.disable_history_clears=opt_disable_history_clears;
		cutGraph.disable_history_clears=opt_disable_history_clears;

		if (opt_adaptive_history_clear > 0) {
			g_under.adaptive_history_clear = true;
			g_over.adaptive_history_clear = true;
			cutGraph.adaptive_history_clear = true;
			g_under.historyClearInterval = opt_adaptive_history_clear;
			g_over.historyClearInterval = opt_adaptive_history_clear;
			cutGraph.historyClearInterval = opt_adaptive_history_clear;
		} else {
			g_under.historyClearInterval = opt_history_clear;
			g_over.historyClearInterval = opt_history_clear;
			cutGraph.historyClearInterval = opt_history_clear;
		}
		g_under.dynamic_history_clears=opt_dynamic_history_clear;
		g_over.dynamic_history_clears=opt_dynamic_history_clear;
		cutGraph.dynamic_history_clears=opt_dynamic_history_clear;
		



		this->rnd_seed=drand(S->getRandomSeed());
        S->addTheory(this);
        graph_decision_reason = S->newReasonMarker(this,true);
	}
	Lit const_true= lit_Undef;
	Lit True(){
		if (const_true==lit_Undef){
			backtrackUntil(0);
			const_true = mkLit(newVar(var(S->True()),-1,false,true));
			enqueueTheory(const_true);
		}
		return const_true;
	}

	void printStats(int detailLevel) {

		
		printf("Graph %d stats:\n", getGraphID());
		/*		printf("Decision Time: %f\n", stats_decision_time);
		 printf("Prop Time: %f (initial: %f)\n", propagationtime,stats_initial_propagation_time);
		 printf("Conflict Time: %f (initial: %f)\n", stats_reason_time,stats_reason_initial_time);*/

		/*	printf("Reach Time: %f (update Time: %f)\n", reachtime,reachupdatetime);
		 printf("Unreach Time: %f (Update Time: %f)\n", unreachtime,unreachupdatetime);
		 printf("Path Time: %f (#Paths: %d, AvgLength %f, total: %d)\n", pathtime, num_learnt_paths, (learnt_path_clause_length /  ((float) num_learnt_paths+1)),learnt_path_clause_length);
		 printf("Min-cut Time: %f (%d calls, %f average, #Cuts: %d, AvgLength %f, total: %d)\n", mctime, stats_mc_calls,(mctime/(stats_mc_calls ? stats_mc_calls:1)),  num_learnt_cuts, (learnt_cut_clause_length /  ((float) num_learnt_cuts+1)),learnt_cut_clause_length);
		 */
		printf("%d nodes, %d edges\n", g_under.nodes(), g_under.edges());
		printf("History Clears: over_approx %ld, under_approx %ld, cut_graph %ld\n", g_over.historyclears,
				g_under.historyclears, cutGraph.historyclears);
		printf("Skipped History Clears: over_approx %ld, under_approx %ld, cut_graph %ld\n", g_over.skipped_historyclears,
				g_under.skipped_historyclears, cutGraph.skipped_historyclears);
		printf("Propagations: %ld (%f s, avg: %f s, %ld skipped)\n", stats_propagations, propagationtime,
				(propagationtime) / ((double) stats_propagations + 1), stats_propagations_skipped);
		printf("Decisions: %ld (%f s, avg: %f s), lazy decisions: %ld\n", stats_decisions, stats_decision_time,
				(stats_decision_time) / ((double) stats_decisions + 1), stats_lazy_decisions);
		printf("Conflicts: %ld (lazy conflicts %ld)\n", stats_num_conflicts,stats_num_lazy_conflicts);
		printf("Reasons: %ld (%f s, avg: %f s)\n", stats_num_reasons, stats_reason_time,
				(stats_reason_time) / ((double) stats_num_reasons + 1));
		printf("enqueues %ld, backtracks %ld (bv enqueues %ld (%ld while sat), bv backtracks %ld)\n",stats_enqueues,stats_backtrack_assigns,stats_bv_enqueues,stats_bv_enqueue_while_sat,stats_bv_backtracks);

		fflush(stdout);

		if (detailLevel > 0) {
			for (Detector * d : detectors)
				d->printStats();
		}
		fflush(stdout);
	}
	
	void writeTheoryWitness(std::ostream& write_to) {
		
		for (Detector * d : detectors) {
			write_to << "Graph " << this->getGraphID() << ", detector " << d->getID() << ":\n";
			d->printSolution(write_to);
		}
		
	}
	
	inline int getTheoryIndex()const {
		return theory_index;
	}
	inline void setTheoryIndex(int id) {
		theory_index = id;
	}
	inline int getGraphID() {
		return getTheoryIndex();
	}
	inline int getTheoryIndexBV(){
		return theory_index;
	}

	bool hasBitVectorEdges()const{
		return has_any_bitvector_edges;
	}

	void setBVTheory(BVTheorySolver<Weight> * bv){
		if(bvTheory!=bv){
			bvTheory = bv;

		if(bv){
				this->propagation_required_theories.push(bv);
			bv->addTheory(this);
		}
	}
	}

	inline bool isEdgeVar(Var v) {
		assert(v < vars.size());
		return vars[v].isEdge;
	}
	inline int getEdgeID(Var v) {
		assert(isEdgeVar(v));
		return vars[v].detector_edge;
	}
	inline int getDetector(Var v) {
		assert(!isEdgeVar(v));
		return vars[v].detector_edge;
	}
	
	inline Var getEdgeVar(int edgeID) {
		Var v = edge_list[edgeID].v;
		assert(v < vars.size());
		assert(vars[v].isEdge);
		return v;
	}
	
	void makeEqual(Lit l1, Lit l2) {
		Lit o1 = toSolver(l1);
		Lit o2 = toSolver(l2);
		tmp_clause.clear();
		tmp_clause.push(~o1);
		tmp_clause.push(o2);
		S->addClauseSafely(tmp_clause);
		tmp_clause.clear();
		tmp_clause.push(o1);
		tmp_clause.push(~o2);
		S->addClauseSafely(tmp_clause);
	}
	void makeEqualInSolver(Lit o1, Lit o2) {
		tmp_clause.clear();
		tmp_clause.push(~o1);
		tmp_clause.push(o2);
		S->addClauseSafely(tmp_clause);
		tmp_clause.clear();
		tmp_clause.push(o1);
		tmp_clause.push(~o2);
		S->addClauseSafely(tmp_clause);
	}
	bool addClause(Lit l1) {
		Lit o1 = toSolver(l1);
		tmp_clause.clear();
		tmp_clause.push(o1);
		S->addClauseSafely(tmp_clause);
		return true;
	}
	bool addClause(Lit l1, Lit l2) {
		Lit o1 = toSolver(l1);
		Lit o2 = toSolver(l2);
		tmp_clause.clear();
		tmp_clause.push(o1);
		tmp_clause.push(o2);

		S->addClauseSafely(tmp_clause);
		return true;
	}
	bool addClause(Lit l1, Lit l2, Lit l3) {
		Lit o1 = toSolver(l1);
		Lit o2 = toSolver(l2);
		Lit o3 = toSolver(l3);
		tmp_clause.clear();
		tmp_clause.push(o1);
		tmp_clause.push(o2);
		tmp_clause.push(o3);
		S->addClauseSafely(tmp_clause);
		return true;
	}
	void addClauseToSolver(Lit l1) {
		tmp_clause.clear();
		tmp_clause.push(l1);
		S->addClauseSafely(tmp_clause);
	}
	void addClauseToSolver(Lit l1, Lit l2) {
		tmp_clause.clear();
		tmp_clause.push(l1);
		tmp_clause.push(l2);

		S->addClauseSafely(tmp_clause);
	}
	void addClauseToSolver(Lit l1, Lit l2, Lit l3) {
		tmp_clause.clear();
		tmp_clause.push(l1);
		tmp_clause.push(l2);
		tmp_clause.push(l3);
		S->addClauseSafely(tmp_clause);
	}

	bool addClause(const vec<Lit> & c) {
		tmp_clause.clear();
		c.copyTo(tmp_clause);
		toSolver(tmp_clause);
		S->addClauseSafely(tmp_clause);
		return true;
	}
	void addClauseSafely(vec<Lit> & c) {
		tmp_clause.clear();
		c.copyTo(tmp_clause);
		toSolver(tmp_clause);
		
		S->addClauseSafely(tmp_clause);
	}
	Var newVar(bool polarity = true, bool dvar = true){
		return newVar(-1,false);//does this shadow the below method, or vice versa?
	}
	Var newVar(int forDetector, bool connectToTheory = false) {
		Var s = S->newVar();
		return newVar(s, forDetector, false, connectToTheory);
	}
	Var newBVVar(Var solverVar, int bvID, int edgeID){
		while (S->nVars() <= solverVar)
			S->newVar();
		Var v = vars.size();
		vars.push();
		vars[v].isEdge = false;
		vars[v].isBV=true;
		vars[v].isTheoryVar=false;
		vars[v].isSatisfied=false;
		vars[v].detector_edge = bvID;
		vars[v].solverVar = solverVar;
		vars[v].theory_var=var_Undef;

		//vars[v].occursPositive=0;
		//vars[v].occursNegative=0;
		assigns.push(l_Undef);
		trail.growTo(v+1);
		S->setTheoryVar(solverVar, getTheoryIndex(), v);
		assert(toSolver(v) == solverVar);

		return v;
	}
	void setDecisionVar(Var solverVar, bool decidable){
		S->setDecisionVar(toSolver(solverVar),decidable);
	}
	bool bvHasDetector(int bvID){
		return bvID<bitvector_data.size() && bitvector_data[bvID].detectorID>=0;
	}
	int getBVDetectorID(int bvID){
		assert(bvHasDetector(bvID));
		return bitvector_data[bvID].detectorID;
	}
	void setBVDetectorID(int bvID, int detectorID){
		bitvector_data.growTo(bvID+1);
		assert(bitvector_data[bvID].edgeID<0);
		assert(bitvector_data[bvID].detectorID<0);
		bitvector_data[bvID].detectorID = detectorID;
	}

	Var newTheoryVar(Var solverVar, int theoryID, Var theoryVar){


		Var v = vars.size();

		S->newTheoryVar(solverVar,getTheoryIndex(),v);


		vars.push();
		vars[v].isEdge = false;
		vars[v].isBV=false;
		vars[v].isTheoryVar=true;
		vars[v].isSatisfied=false;
		vars[v].detector_edge = theoryID;
		vars[v].solverVar = solverVar;
		vars[v].theory_var=theoryVar;
		//vars[v].occursPositive=0;
		//vars[v].occursNegative=0;
		trail.growTo(v+1);
		assigns.push(l_Undef);
		return v;
	}
	Var newVar(Var solverVar, int detector, bool isEdge = false, bool connectToTheory = true) {
		while (S->nVars() <= solverVar)
			S->newVar();

		if(S->hasTheory(solverVar) &&  S->getTheoryID(solverVar)==getTheoryIndex()){
			return S->getTheoryVar(solverVar);
		}

		Var v = vars.size();
		vars.push();
		vars[v].isEdge = isEdge;
		vars[v].isBV=false;
		vars[v].isTheoryVar=false;
		vars[v].isSatisfied=false;
		vars[v].detector_edge = detector;
		vars[v].solverVar = solverVar;
		vars[v].theory_var=var_Undef;
		//vars[v].occursPositive=0;
		//vars[v].occursNegative=0;
		trail.growTo(v+1);
		assigns.push(l_Undef);

		if (connectToTheory) {
			S->setTheoryVar(solverVar, getTheoryIndex(), v);
			assert(toSolver(v) == solverVar);
		}
		if (!isEdge && detector >= 0)
			detectors[detector]->addVar(v);
		return v;
	}
	inline int level(Var v)const {
		return S->level(toSolver(v));
	}
	inline int decisionLevel()const {
		return decisions.size()-1; //S->decisionLevel();
	}
	inline int nVars() const {
		return vars.size(); //S->nVars();
	}
	inline Var toSolver(Var v)const {
		//return v;
		assert(v < vars.size());
		//assert(S->hasTheory(vars[v].solverVar));
		//assert(S->getTheoryVar(vars[v].solverVar)==v);
		return vars[v].solverVar;
	}
	
	inline Lit toSolver(Lit l)const {
		if(l==lit_Undef)
			return lit_Undef;
		//assert(S->hasTheory(vars[var(l)].solverVar));
		//assert(S->getTheoryVar(vars[var(l)].solverVar)==var(l));
		return mkLit(vars[var(l)].solverVar, sign(l));
	}
	
	void toSolver(vec<Lit> & c) {
		for (int i = 0; i < c.size(); i++) {
			c[i] = toSolver(c[i]);
		}
	}
	
	double & getRandomSeed()override{
		return rnd_seed;
	}
	
	inline bool edgeWeightDecidable(int edgeID,DetectorComparison op, Weight edgeWeight) {
		if (! hasBitVector(edgeID) || !opt_decide_graph_bv)
			return false;
		int bvID = getEdgeBV(edgeID).getID();
		Comparison bvOp;
		if (op==DetectorComparison::leq){
			bvOp=Comparison::leq;
			return bvTheory->decidableBV(bvOp,bvID,edgeWeight);

		}else if (op==DetectorComparison::lt){
			bvOp=Comparison::lt;
			return bvTheory->decidableBV(bvOp, bvID, edgeWeight);
		}else if (op==DetectorComparison::geq){
			/*printf("decide graph %d edge %d bv %d >= ",this->getTheoryIndex(), getEdgeBV(edgeID).getID(),edgeID);
			std::cout << edgeWeight <<"\n";*/
			bvOp=Comparison::geq;
			return bvTheory->decidableBV(bvOp, bvID, edgeWeight);
		}else if (op==DetectorComparison::gt){
			bvOp=Comparison::gt;
			return bvTheory->decidableBV(bvOp, bvID, edgeWeight);
		}else if (op==DetectorComparison::eq){
			return bvTheory->decidableBV(Comparison::leq, bvID, edgeWeight) || bvTheory->decidableBV(Comparison::geq, bvID, edgeWeight);

		}else{
			assert(false);
			return false;
		}

	}

	inline bool decidable(Var v)const{
		return S->value(toSolver(v))==l_Undef;
	}
	inline bool decidable(Lit l)const{
		return S->value(toSolver(var(l)))==l_Undef;
	}

	inline lbool value(Var v) const{
		return assigns[v]; //S->value(toSolver(v));
	}
	inline lbool value(Lit l)const {
		return assigns[var(l)] ^ sign(l);
	}
	inline lbool dbg_value(Var v) {
		return S->value(toSolver(v));
	}
	inline lbool dbg_value(Lit l) {
		return S->value(toSolver(l));
	}
	inline bool enqueue(Lit l, CRef reason) {
		assert(assigns[var(l)]==l_Undef);
		
		Lit sl = toSolver(l);
		if (S->enqueue(sl, reason)) {
			enqueueTheory(l);//is this still needed?
			return true;
		} else {
			return false;
		}
	}
	struct BVAssign{
		Var graphAssign;//last assignment on the graph theory trail at the time of assignment
		int bvTrail;
		int bvID:31;
		int theory_is_satisfied:1;
		int level;//this is redundant and can likely be removed
	};
	vec<BVAssign> bvtrail;//connects the graph theory and bv theory trails, for conflict analysis.

	inline bool assignBV(int bvID, Comparison comp,Weight value,typename BVTheorySolver<Weight>::Operation*bvOp){
		dbg_check_trails();
		if(bvTheory && opt_graph_bv_prop){
			int bvTrailSize = bvTheory->trail.size();

			bool conflict = !bvTheory->assignBV(bvID,comp,value,*bvOp);
			if(bvTheory->trail.size()>bvTrailSize){
				int lev = decisionLevel();
				Var v = getBack(lev);
				//temporarily backtrack until the graphTrailPos
				while(v==-1 && lev>0 ){
					lev--;
					v = getBack(lev);
				}
				//if this enqueue was successful, AND it led to a refinment in the bvTheory
				bvtrail.push({v ,bvTrailSize,bvID,lev});
				dbg_check_trails();
			}
			return !conflict;
		}
		return true;
	}

	class GraphTheoryOp: public  BVTheorySolver<Weight>::Operation{
		using BVTheorySolver<Weight>::Operation::getID;
		using BVTheorySolver<Weight>::Operation::theory;

		 GraphTheorySolver * outer;
	public:
		GraphTheoryOp(BVTheorySolver<Weight> &theory,GraphTheorySolver * outer):BVTheorySolver<Weight>::Operation(theory),outer(outer){

		}
		 typename BVTheorySolver<Weight>::OperationType getType()const override{
			return  BVTheorySolver<Weight>::OperationType::cause_is_theory;
		}


		int getBV()override=0;

		bool propagate(bool & changed, vec<Lit> & conflict)override{return true;}

	    void updateApprox(Var ignore_bv, Weight & under_new, Weight & over_new,typename BVTheorySolver<Weight>::Cause & under_cause_new, typename BVTheorySolver<Weight>::Cause & over_cause_new)override{}

		virtual void analyzeReason(bool compareOver,Comparison op, Weight  to,  vec<Lit> & conflict)override{
			outer->dbg_check_trails();
			int bvTrailPos = theory.analysis_trail_pos;//current position in the bitvector trail that analysis is occuring at.
			//lookup the corresponding position in the graph theory trail that we should be applying the bv theory analysis
			bool found=false;
			Var lastAssign=var_Undef;
			int graphLevel=-1;
			const vec<BVAssign> & bvtrail = outer->bvtrail;
			for(int i = bvtrail.size()-1;i>=0;i--){

				int bvTrail = bvtrail[i].bvTrail;
				if(bvTrail<=bvTrailPos){
					found=true;
					graphLevel = bvtrail[i].level;
					lastAssign =  bvtrail[i].graphAssign;
					break;
				}
			}
			if(!found){
				throw std::runtime_error("Internal error in bv/graph clause learning");
			}

			outer->rewindUntil(lastAssign);
		}
		void completeAnalysis(){
			outer->undoRewind();
		}

		bool checkSolved()override{	return true;}
	};

	~GraphTheorySolver() {
		for(int i = 0;i<node_symbols.size();i++){
			if(node_symbols[i]){
				delete(node_symbols[i]);
	}
		}
		node_symbols.clear();
		for(int i = 0;i<edge_symbols.size();i++){
			if(edge_symbols[i]){
				delete(edge_symbols[i]);
			}
		}
		edge_symbols.clear();
	}
	void setNodeName(int node,const char * symbol){
		node_symbols.growTo(node+1,nullptr);
		node_symbols[node]=strdup(symbol);
	}
	const char * getNodeName(int node){
		if(node>=node_symbols.size())
			return nullptr;
		else
			return node_symbols[node];
	}
	void setEdgeName(Var edgeVar,const char * symbol){
		int edgeID = getEdgeID(edgeVar);
		edge_symbols.growTo(edgeID+1,nullptr);
		edge_symbols[edgeID]=strdup(symbol);
	}
	const char * getEdgeName(Var edgeVar){
		int edgeID = getEdgeID(edgeVar);
		if(edgeID>=edge_symbols.size())
			return nullptr;
		else
			return edge_symbols[edgeID];
	}
	int newNode() {
		
		inv_adj.push();
		undirected_adj.push();
		reach_info.push();
		connect_info.push();
		dist_info.push();
		weighted_dist_info.push();
		g_over.addNode();
		cutGraph.addNode();
		g_under_weights_over.addNode();
		g_over_weights_under.addNode();
		seen.growTo(nNodes());
		
		return g_under.addNode();
	}
	void newNodes(int n) {
		for (int i = 0; i < n; i++)
			newNode();
	}
	int nNodes() {
		return g_under.nodes();
	}
	bool isNode(int n) {
		return n >= 0 && n < nNodes();
	}
	
	bool hasBitVector(int edgeID){
		return edge_list[edgeID].bvID>=0;
	}
	BitVector<Weight> getBV(int bvID){
		return bvTheory->getBV(bvID);
	}
	BitVector<Weight> getEdgeBV(int edgeID){
		return bvTheory->getBV(edge_list[edgeID].bvID);
	}

	bool dbg_propgation(Lit l) {
#ifdef DEBUG_GRAPH
		static vec<Lit> c;
		c.clear();
		for (int i = 0; i < S->trail.size(); i++) {
			if (!S->hasTheory(S->trail[i]) || S->getTheoryID(S->trail[i]) != getTheoryIndex())
				continue;
			Lit l = S->getTheoryLit(S->trail[i]);
			Var v = var(l);
			if (isEdgeVar(v)) {
				Edge & e = edge_list[getEdgeID(v)];
				c.push(l);
			}
			
			/*		if(v>=min_edge_var && v<min_edge_var+num_edges){
			 if(edge_list[v-min_edge_var].v<0)
			 continue;
			 Edge e = edge_list[v-min_edge_var];
			 c.push(l);
			 }*/
		}
		c.push(~l);
		//bool res = dbg->solve(c);
		//assert(~res);
#endif
		return true;
	}
	
	void dbg_sync_reachability() {
/*
#ifdef DEBUG_GRAPH
		
		for(int i = 0;i<reach_detectors.size();i++) {
			ReachDetector* d = reach_detectors[i];
			d->dbg_sync_reachability();
		}
#endif*/

	}

	void dbg_sync() {

#ifdef DEBUG_GRAPH


#endif
	}
	void dbg_full_sync() {

#ifdef DEBUG_GRAPH
		dbg_sync();
		for(int i =0;i<edge_list.size();i++) {

			if(edge_list[i].edgeID>=0 && g_under.edgeEnabled(i)) {
				assert(value(edge_list[i].v)==l_True);
			} else if (edge_list[i].edgeID>=0 && !g_over.edgeEnabled(i)) {
				assert(value(edge_list[i].v)==l_False);
			}
		}

#endif
	}
	void backtrackAssign(Lit l){
		stats_backtrack_assigns++;
		assert(l!=lit_Undef);
		Var v = var(l);
		assert(value(l)==l_True);
		lbool assign = sign(l)?l_False:l_True;
			if (isEdgeVar(v)) {
				int edge_num = getEdgeID(v); //e.var-min_edge_var;
				assert(assigns[v]!=l_Undef);

				if (assign==l_True) {
					g_under.disableEdge(edge_num);

					assert(!cutGraph.edgeEnabled(edge_num * 2));
				} else {
					g_over.enableEdge(edge_num);
					if (opt_conflict_min_cut) {
						assert(cutGraph.edgeEnabled(edge_num * 2));
						cutGraph.disableEdge(edge_num * 2);
						assert(!cutGraph.edgeEnabled(edge_num * 2 + 1));
						cutGraph.enableEdge(edge_num * 2 + 1);
					}
				}
				if(using_neg_weights){
					if (assign==l_True) {
						g_under_weights_over.disableEdge(edge_num);
					} else {
						g_over_weights_under.enableEdge(edge_num);
					}
				}
			} else {
				detectors[getDetector(v)]->unassign(mkLit(v, assign==l_False));
			}
			assigns[v] = l_Undef;
		}

	//vec<Lit> to_reenqueue;
	void backtrackUntil(int untilLevel) {
		static int it = 0;
		++it;
		undoRewind();

		//printf("g%d: backtrack until level %d\n", this->id,untilLevel);
		//assert(to_reenqueue.size()==0);
		bool changed = false;

		while(satisfied_lits.size() && satisfied_lits.last().level>untilLevel){
			int lev = satisfied_lits.last().level;
			assert(lev>untilLevel);
			Lit l = satisfied_lits.last().l;
			assert(value(l)==l_True);
			assert(vars[var(l)].isSatisfied);
			vars[var(l)].isSatisfied=false;
			satisfied_lits.pop();
			int detector = getDetector(var(l));
			if(detector>=0) {

				detectors[detector]->setSatisfied(l, false);
				if(satisfied_detectors[detector] && !detectors[detector]->detectorIsSatisfied()){
					satisfied_detectors[detector]=false;
					n_satisfied_detectors--;
					assert(n_satisfied_detectors>=0);
				}
			}

		}

		//first, undo any assignments in the lazy trail
		if(lazy_trail_head!=var_Undef){
			requiresPropagation = true;
			S->needsPropagation(getTheoryIndex());
			Var v = getPrev(lazy_trail_head);
			while(true){
				Lit l = mkLit(v, value(v)==l_False);
				assert(assigns[v]!=l_Undef);

	/*			if(S->level(toSolver(v))<=untilLevel){
					if(value(v) != S->value(toSolver(v))){
						changed=true;
					}
					to_reenqueue.push(mkLit(v, S->value(toSolver(v))==l_False));//re-enqueue it with the SAT SOLVERS assignment, not its assignment on the trail, which may be wrong!;
				}else{*/
					backtrackAssign(l);
					changed = true;
				//}
				Var p = _getPrev(v);
				if (removeFromTrail(v)){
					break;
				}
				v=p;
			}
		}

		if(decisionLevel()>untilLevel){
		//as we backtrack, we need to remove and add edges in the two graphs accordingly.
			while (decisionLevel() > untilLevel) {
				//int stop = getDecision(untilLevel);
				//Var decision = getDecision(decisionLevel());

				Var v = getBack(decisionLevel());
				//for (int i = trail.size() - 1; i >= trail_lim[untilLevel]; i--) {
				while(v!=var_Undef){
					Lit l = mkLit(v, value(v)==l_False);
					assert(assigns[v]!=l_Undef);
					if(assigns[v]==l_Undef){
						throw std::runtime_error("Internal error in backtracking");
					}
			/*		if(S->level(toSolver(v))<=untilLevel){
						if(value(v) != S->value(toSolver(v))){
							changed=true;
						}
						to_reenqueue.push(mkLit(v, S->value(toSolver(v))==l_False));//re-enqueue it with the SAT SOLVERS assignment, not its assignment on the trail, which may be wrong!;
					}else{*/
						backtrackAssign(l);
						changed = true;
					//}
					Var p = _getPrev(v);
					if (_removeFromTrail(v,decisionLevel())){
						p=var_Undef;
					}
					v=p;
				}
				decisions.pop();
			}

			assert(decisionLevel() == untilLevel);

			for (Detector * d : detectors) {
				d->backtrack(untilLevel);
			}
		}

		if (changed) {
			requiresPropagation = true;
			S->needsPropagation(getTheoryIndex());
		}
/*		while(to_reenqueue.size()){
			Lit p = to_reenqueue.last();
			to_reenqueue.pop();
			assert(S->level(toSolver(var(p))) <=untilLevel);
			enqueueTheory(p);
		}*/

		for (int i = 0; i < theories.size(); i++) {
			theories[i]->backtrackUntil(untilLevel);
		}

		assert(dbg_graphsUpToDate());
		//dbg_sync();
	}


	void backtrackUntil(Lit p) {
		static int it=0;
		++it;
		undoRewind();
		//printf("g%d : backtrack until lit %d\n", this->id,dimacs(p));
		//need to remove and add edges in the two graphs accordingly.
		assert(onTrail(var(p))||onLazyTrail(var(p)));

		//is this safe? there is a bit of a problem here, since I don't know where in the trail the 'satisfied'
		//information was enqueued (only the level at which it was enqueued).
		//on the other hand, a satisfied lit shouldn't be involved in a conflict, and
		//everything should end up correctly in sync after the solver backtracks post-conflict, so this will
		//*hopefully* not matter...
		int untilLevel = level(var(p));
		while(satisfied_lits.size() && satisfied_lits.last().level>untilLevel){
			int lev = satisfied_lits.last().level;
			assert(lev>untilLevel);
			Lit l = satisfied_lits.last().l;
			assert(value(l)==l_True);
			assert(vars[var(l)].isSatisfied);
			vars[var(l)].isSatisfied=false;
			satisfied_lits.pop();
			int detector = getDetector(var(l));
			if(detector>=0) {

				detectors[detector]->setSatisfied(l, false);
				if(satisfied_detectors[detector] && !detectors[detector]->detectorIsSatisfied()){
					satisfied_detectors[detector]=false;
					n_satisfied_detectors--;
					assert(n_satisfied_detectors>=0);
				}
			}

		}

		Var v;
		if(!onLazyTrail(var(p))){

			backtrackUntil(level(var(p)));//this is neccesary!
			assert(trail[var(p)].level==decisionLevel());
			v= getBack(decisionLevel());
		}else{
			assert(lazy_trail_head!=var_Undef);
			v = getPrev(lazy_trail_head);
		}

		//assert(to_reenqueue.size()==0);


		while(v!=var(p)){
			Lit l = mkLit(v, value(v)==l_False);
			assert(assigns[v]!=l_Undef);

		/*	if(S->level(toSolver(v))<decisionLevel()){//strict less than is intentional, here, as we will always backtrack past this level after backtrackUntil(p).
				to_reenqueue.push(mkLit(v, S->value(toSolver(v))==l_False));//re-enqueue it with the SAT SOLVERS assignment, not its assignment on the trail, which may be wrong!;
			}else{*/
				backtrackAssign(l);
			//}
			Var p = _getPrev(v);
			if (removeFromTrail(v)){
				break;
			}
			v=p;
		}
		requiresPropagation = true;//is this always required?
		S->needsPropagation(getTheoryIndex());

		for (Detector * d : detectors) {
			d->backtrack(this->decisionLevel());
		}

	/*	while(to_reenqueue.size()){
			//this is safe to combine with backtrackUntil(p), because only vars at strictly earlier levels are re_enqueued here.
			Lit p = to_reenqueue.last();
			to_reenqueue.pop();
			assert(S->level(toSolver(var(p))) <decisionLevel());
			enqueueTheory(p);
		}
*/
		for (int i = 0; i < theories.size(); i++) {
			theories[i]->backtrackUntil(this->decisionLevel());
		}


	}


	vec<Lit> rewound_assigns;
	void rewindUntil(Var until){
		dbg_check_trails();
		if(decisionLevel()==0)
			return;
		if(until==-1){
			until = getBack(0);
		}
		assert(onTrail(until)||onLazyTrail(until));
		assert(rewound_assigns.size()==0);
		int lev;
		Var v;
		if(lazy_trail_head==var_Undef){
			v= getBack(decisionLevel());
			lev = decisionLevel();
		}else{
			lev=decisionLevel()+1;
			assert(lazy_trail_head!=var_Undef);
			v = getPrev(lazy_trail_head);
		}
		while(lev>0){

			//assert(to_reenqueue.size()==0);
			while(v!=var_Undef){
				if(v==until){
					return;
				}
				Lit l = mkLit(v, value(v)==l_False);
				rewound_assigns.push(l);
				assert(assigns[v]!=l_Undef);
				backtrackAssign(l);
				Var p = _getPrev(v);
				if(p==v || p==getBack(lev))
					break;
				v=p;
			}
			lev--;
			v = getBack(lev);
		}
	}

	void undoRewind(){
		while(rewound_assigns.size()){
			Lit l =rewound_assigns.last();
			rewound_assigns.pop();
			_assign(l);
		}
	}

	void undecideBV(int bvID){

		if(isEdgeBV(bvID)){
			int edgeID = getBVEdge(bvID);
			for (int i = 0; i < detectors.size(); i++) {
				Detector * r = detectors[i];
				r->undecideEdgeWeight(edgeID);
			}
		}
	}
	void undecideTheory(Lit l){
		//assert(value(l)==l_True); //the value can be locally unassigned if we backtracked while building a propagation reason
		//The value can also be unassigned if the theory was marked satisfied

		if(supportsLazyBacktracking()){
			prependToLazyTrail(l);

		}

		for (int i = 0; i < detectors.size(); i++) {
			Detector * r = detectors[i];
			r->undecide(l);
		}

	}
	bool supportsDecisions()override {
		return true;
	}
	Lit decideTheory(CRef & decision_reason) {
		if (!opt_decide_theories)
			return lit_Undef;
		double start = rtime(1);
		static int iter = 0;
		iter++;



        //dbg_full_sync();
		if(opt_lazy_backtrack && supportsLazyBacktracking() && opt_lazy_backtrack_decisions && detectors.size()){//the detectors.size() check is a hack, to prevent empty graphs from forcing the decisions that they didn't originally contribute to.
			//assert(n_decisions<=decisionLevel());
			//printf("g%d lazy dec start: decisionLevel %d, decisions %d\n", this->id, decisionLevel(),n_decisions);
			//when redeciding a literal, should check to see whether it would still be recomended as a decision by its detector...
			if(lazy_trail_head!=var_Undef){
				assert(value(lazy_trail_head)!=l_Undef);
				assert(S->value(toSolver(lazy_trail_head))==l_Undef);
				Lit d = mkLit(lazy_trail_head, value(lazy_trail_head)==l_False);
				Lit solverLit = toSolver(d);

				stats_lazy_decisions++;
				stats_decisions++;
				//printf("g%d: graph lazy decision %d: %d\n", this->id, iter, dimacs(d));
				stats_decision_time += rtime(1) - start;
				return solverLit;
			}
		}
        if (!opt_monolothic_theory_decisions)
            return lit_Undef;

		for (int i = 0; i < detectors.size(); i++) {
			Detector * r = detectors[i];
            if(satisfied_detectors[r->getID()])
                continue;
			decision_reason = CRef_Undef;
            Lit l = r->decide(decision_reason);
			if (l != lit_Undef) {

				if(opt_decide_graph_bv && !sign(l) && isEdgeVar(var(l)) && hasBitVector(getEdgeID(var(l))) && r->supportsEdgeDecisions()){
					int edgeID = getEdgeID(var(l));
					EdgeDecider<Weight> * d = dynamic_cast<EdgeDecider<Weight> *>(r); //(EdgeDecider<Weight>*)r;
					Weight edgeWeight=-1;
					DetectorComparison op;
					if(d->decideEdgeWeight(edgeID,edgeWeight,op)){
						assert(edgeWeight>=0);
						Lit bv_decision = lit_Undef;

						Comparison bvOp;
						if (op==DetectorComparison::leq){
							bvOp=Comparison::leq;
                            bv_decision = bvTheory->decideBV(bvOp, getEdgeBV(edgeID).getID(), edgeWeight);


						}else if (op==DetectorComparison::lt){
							bvOp=Comparison::lt;
                            bv_decision = bvTheory->decideBV(bvOp, getEdgeBV(edgeID).getID(), edgeWeight);
						}else if (op==DetectorComparison::geq){
							/*printf("decide graph %d edge %d bv %d >= ",this->getTheoryIndex(), getEdgeBV(edgeID).getID(),edgeID);
							std::cout << edgeWeight <<"\n";*/
							bvOp=Comparison::geq;
                            bv_decision = bvTheory->decideBV(bvOp, getEdgeBV(edgeID).getID(), edgeWeight);
						}else if (op==DetectorComparison::gt){
							bvOp=Comparison::gt;
                            bv_decision = bvTheory->decideBV(bvOp, getEdgeBV(edgeID).getID(), edgeWeight);
						}else if (op==DetectorComparison::eq){
                            bv_decision = bvTheory->decideBV(Comparison::leq, getEdgeBV(edgeID).getID(), edgeWeight);
							if(bv_decision==lit_Undef)
                                bv_decision = bvTheory->decideBV(Comparison::geq, getEdgeBV(edgeID).getID(), edgeWeight);
						}else{
							exit(1);//ne not supported yet...
						}

						if(bv_decision!=lit_Undef){
                            assert(S->value(bv_decision)==l_Undef);
							stats_decisions++;
							r->undecide(l);
							stats_decision_time += rtime(1) - start;
                            if(S->value(bv_decision)!=l_Undef){

                                printf("%d %d\n",var(l), dimacs(bv_decision));
                                throw std::runtime_error("error in decision heuristic");
                            }
							return bv_decision;
						}
					}
				}
                assert(l==lit_Undef || value(l)==l_Undef);
                assert(l==lit_Undef || S->value(toSolver(l))==l_Undef);
				stats_decisions++;
				r->stats_decisions++;
				stats_decision_time += rtime(1) - start;
				if(opt_verb>2)
					printf("g%d: graph decision %d: %d\n", this->getTheoryIndex(), iter, dimacs(l));
                assert(l==lit_Undef || value(l)==l_Undef);
                assert(l==lit_Undef || S->value(toSolver(l))==l_Undef);
				return toSolver(l);
			}
		}
		stats_decision_time += rtime(1) - start;
		return lit_Undef;
	}
	

	void newDecisionLevel() {
		//trail_lim.push(trail.size());
		decisions.push(var_Undef);
		assert(decisionLevel()<=S->decisionLevel());
	}


	void buildBVReason(int bvID, Comparison comp, Weight compareTo, vec<Lit> &reason){
		static int iter = 0;
		//todo: optimize this for case where bv is statically known to satisfy or fail the constraint...
		BitVector<Weight> bv = bvTheory->getBV(bvID);
		Lit c = getBV_COMP(bvID,-comp,compareTo);
		assert(dbg_value(c)==l_False);
		reason.push(c);
	}

	void buildReason(Lit p, vec<Lit> & reason,CRef marker) {
		//CRef marker = S->reason(var(toSolver(p)));
		assert(marker != CRef_Undef);
		int pos = CRef_Undef - marker;
		if(marker_map[pos].forTheory){
			int d = marker_map[pos].id;
			//double initial_start = rtime(1);
			double start = rtime(1);
			assert(d < detectors.size());
			theories[d]->buildReason(p, reason,marker);
			//toSolver(reason);
			double finish = rtime(1);
			stats_reason_time += finish - start;
			stats_num_reasons++;
		}else{
		
			int d = marker_map[pos].id;
			//double initial_start = rtime(1);
			double start = rtime(1);
			backtrackUntil(p);

			assert(d < detectors.size());
			detectors[d]->buildReason(p, reason, marker);
			//toSolver(reason);
			double finish = rtime(1);
			stats_reason_time += finish - start;
			stats_num_reasons++;
			//stats_reason_initial_time+=start-initial_start;
		}
	}
	
	bool dbg_reachable(int from, int to, bool undirected = false) {
#ifdef DEBUG_DIJKSTRA
		
		if(undirected) {
			UnweightedDijkstra<Weight,Distance<int>::NullStatus, true> d(from,g_under);
			d.update();
			return d.connected(to);
		} else {
			UnweightedDijkstra<Weight> d(from,g_under);
			d.update();
			return d.connected(to);
		}

#else
		return true;
#endif
	}
	
	bool dbg_notreachable(int from, int to, bool undirected = false) {
		
#ifdef DEBUG_GRAPH
		//drawFull(from,to);
		
		/*DynamicGraph<Weight> g;
		for (int i = 0; i < nNodes(); i++) {
			g.addNode();
		}
		
		for (int i = 0; i < edge_list.size(); i++) {
			if (edge_list[i].v < 0)
				continue;
			Edge e = edge_list[i];
			if (value(e.v) != l_False) {
				Weight w = edge_weights[e.edgeID];
				g.addEdge(e.from, e.to,e.edgeID,w);
			}
		}
		if (undirected) {
			UnweightedDijkstra<Weight,typename Distance<int>::NullStatus, true> d(from, g);
			
			return !d.connected(to);
		} else {
			UnweightedDijkstra<Weight,typename Distance<int>::NullStatus, false> d(from, g);
			
			return !d.connected(to);
		}*/
#endif
		return true;
		
	}
	
	bool dbg_graphsUpToDate() {
#ifdef DEBUG_GRAPH2
		for(int i = 0;i<edge_list.size();i++) {
			if(edge_list[i].v<0)
			continue;
			Edge e = edge_list[i];
			lbool val = value(e.v);

			if(val==l_True || val==l_Undef) {
				assert(g_over.edgeEnabled(i));
				if(using_neg_weights){
					if(!g_over_weights_under.edgeEnabled(i)){
						exit(3);
					}
				}
				//assert(antig.hasEdge(e.from,e.to));
			} else {
				assert(!g_over.edgeEnabled(i));
				if(using_neg_weights){
					if(g_over_weights_under.edgeEnabled(i)){
						exit(3);
					}
				}
				//assert(!antig.hasEdge(e.from,e.to));
			}
			if(val==l_True) {
				assert(g_under.edgeEnabled(i));
				if(!g_under.edgeEnabled(i)){
					exit(3);
				}
				if(using_neg_weights){
					if(!g_under_weights_over.edgeEnabled(i)){
						exit(3);
					}
				}
				//assert(g.hasEdge(e.from,e.to));
			} else {
				if(g_under.edgeEnabled(i)){
					exit(3);
				}
				assert(!g_under.edgeEnabled(i));
				if(using_neg_weights){
					if(g_under_weights_over.edgeEnabled(i)){
						exit(3);
					}
				}
				//assert(!g.hasEdge(e.from,e.to));
			}
		}

#endif
		return true;
	}
	
	/*	int getEdgeID(Var v){
	 assert(v>= min_edge_var && v<min_edge_var+edge_list.size());

	 //this is an edge assignment
	 int edge_num = v-min_edge_var;
	 return edge_num;
	 }*/

	void preprocess() {

		for (int i = 0; i < detectors.size(); i++) {
			detectors[i]->preprocess();
		}
		/*g_under.clearHistory(true);
		g_over.clearHistory(true);
		g_under_weights_over.clearHistory(true);
		g_over_weights_under.clearHistory(true);
		cutGraph.clearHistory(true);
		g_under.clearChanged();
		g_over.clearChanged();
		cutGraph.clearChanged();*/

		if(opt_lazy_backtrack){
			lazy_backtracking_enabled=true;
			//currently, lazy backtracking is only supported if _all_ property lits are ground.
			for (Detector * d:detectors){
				if(d->unassigned_negatives >0 && d->unassigned_positives>0){
					//at least one property lit of this detector is unassigned, so disable lazy_backtracking.
					lazy_backtracking_enabled=false;
					break;
				}
			}
		}

	}
	void setLiteralOccurs(Lit l, bool occurs) {
		if (isEdgeVar(var(l))) {
			//don't do anything
		} else {
			//this is a graph property detector var
			//if (!sign(l) && vars[var(l)].occursPositive != occurs)
				detectors[getDetector(var(l))]->setOccurs(l, occurs);
			//else if (sign(l) && vars[var(l)].occursNegative != occurs)
			//	detectors[getDetector(var(l))]->setOccurs(l, occurs);
		}
		
	}
	
	void enqueueBV(int bvID){
		if(!S->model.size() && theoryIsSatisfied()){
			stats_bv_enqueue_while_sat++;
		}
		if(!S->model.size())
			stats_bv_enqueues++;

		int lev = bvTheory->decisionLevel();//decision level must be synced with bvtheory to ensure the correctness of the enqueueSat method.
		while (lev > decisionLevel()) {
            newDecisionLevel();
        }
		requiresPropagation=true;
		S->needsPropagation(getTheoryIndex());
		if(isEdgeBV(bvID)){
			int edgeID = getBVEdge(bvID);
			if(edgeID==11){
				int a=1;
			}
/*			if((!S->model.size()) && g_under.getEdgeWeight(edgeID) == edge_bv_weights[edgeID].getUnder() && g_over.getEdgeWeight(edgeID) == edge_bv_weights[edgeID].getOver()){
				printf("useless edge weight update: graph %d edge %d bv %d\n",getTheoryIndex(),edgeID,bvID);
			}*/
			g_under.setEdgeWeight(edgeID,edge_bv_weights[edgeID].getUnder());
			g_over.setEdgeWeight(edgeID, edge_bv_weights[edgeID].getOver());
			if(using_neg_weights){
				g_under_weights_over.setEdgeWeight(edgeID,edge_bv_weights[edgeID].getOver());
				g_over_weights_under.setEdgeWeight(edgeID, edge_bv_weights[edgeID].getUnder());
			}

		}else if(bvHasDetector(bvID)){
			int detectorID = getBVDetectorID(bvID);
			assert(detectorID>=0);assert(detectorID<detectors.size());assert(detectors[detectorID]);
			detectors[detectorID]->assignBV(bvID);
		}
	}
	void backtrackBV(int bvID){
		if (isEdgeBV(bvID)){
			int edgeID = getBVEdge(bvID);
			for (int i = 0; i < detectors.size(); i++) {
				Detector * r = detectors[i];
				r->undecideEdgeWeight(edgeID);
			}
			g_under.setEdgeWeight(edgeID,edge_bv_weights[edgeID].getUnder());
			g_over.setEdgeWeight(edgeID, edge_bv_weights[edgeID].getOver());
			if(using_neg_weights){
				g_under_weights_over.setEdgeWeight(edgeID,edge_bv_weights[edgeID].getOver());
				g_over_weights_under.setEdgeWeight(edgeID, edge_bv_weights[edgeID].getUnder());
			}

		}else if(bvHasDetector(bvID)){
			int detectorID = getBVDetectorID(bvID);
			assert(detectorID>=0);assert(detectorID<detectors.size());assert(detectors[detectorID]);
			detectors[detectorID]->unassignBV(bvID);
		}
	}
	//mark an atom as satisfied in the theory, so it doesn't need to be tracked in the future
	void enqueueSat(Lit l){
		assert(value(l)==l_True);//the literal must be assigned true
		if(opt_detect_satisfied_predicates) {
			if (!vars[var(l)].isSatisfied) {
				//int lev = S->decisionLevel();
				/*while (lev > decisionLevel()) {
					newDecisionLevel();
				}*/
				while (decisionLevel()<S->decisionLevel()) {
					newDecisionLevel();
				}
				satisfied_lits.push(SatisfiedLit(decisionLevel(), l));
				vars[var(l)].isSatisfied = 1;
				int d = getDetector(var(l));
				detectors[d]->setSatisfied(l, true);
				if(!satisfied_detectors[d] && detectors[d]->detectorIsSatisfied()){
					//if edge sets are used, also set any equivalent detectors in the other edge sets to satisfied.
					n_satisfied_detectors++;
					assert(n_satisfied_detectors<=detectors.size());
					satisfied_detectors[d]=true;
				}

			}
		}
	}
	bool isSatisfied(Lit l){
		assert(!(vars[var(l)].isSatisfied && value(l)!=l_True ));//if the var is marked satisfied, it SHOULD be the case
		//that it is assigned true.
		return vars[var(l)].isSatisfied;
	}
	bool theoryIsSatisfied() override{
		return n_satisfied_detectors==detectors.size();
	}


	void enqueueTheory(Lit l) {
		Var v = var(l);
		stats_enqueues++;
		int lev = level(v);//level from the SAT solver.
		if(!opt_lazy_backtrack){
			assert(decisionLevel() <= lev);
		}

		while (lev > decisionLevel()) {
			newDecisionLevel();
		}

		//if we are assigning lazily, then there are additional possibilities.
		//bool on_trail=false;
		if (value(v)==S->value(toSolver(v))) {
			if(value(v)==l_Undef){
				throw std::runtime_error("Internal error in graph enqueue");
			}
			if(lev == decisionLevel()) {
			//this is already enqueued.
			//place it into the correct place on the trail
			removeFromTrail(var(l));
			appendToTrail(l,decisionLevel());
			}
			return;
		}else if (opt_lazy_backtrack && value(v)!=l_Undef){
			if(!opt_lazy_backtrack){
				assert(decisionLevel() <= lev);
				if(decisionLevel() > lev){
					throw std::runtime_error("Internal error in graph enqueue");
				}
			}
			assert(value(v)!=S->value(toSolver(v)));
			//this literal was already assigned, and then we backtracked _lazily_ without unassigning it in the theory solver.
			//unassign it now, by itself.
			bool assign = sign(l);//intentionally inverting this compared to what it normally would be, because l is currently assigned with the opposite polarity in the theory solver
			assigns[v] = l_Undef;
			//on_trail=true;
			if (isEdgeVar(v)) {
				int edge_num = getEdgeID(v); //e.var-min_edge_var;
				if (assign) {
					g_under.disableEdge( edge_num);
					assert(!cutGraph.edgeEnabled(edge_num * 2));
				} else {
					g_over.enableEdge(edge_num);

					if (opt_conflict_min_cut) {
						assert(cutGraph.edgeEnabled(edge_num * 2));
						cutGraph.disableEdge( edge_num * 2);
						assert(!cutGraph.edgeEnabled(edge_num * 2 + 1));
						cutGraph.enableEdge(edge_num * 2 + 1);
					}
				}
				if(using_neg_weights){
					if (assign) {
						g_under_weights_over.disableEdge( edge_num);
					} else {
						g_over_weights_under.enableEdge( edge_num);
					}
				}

			} else {
				//This is a reachability literal
				detectors[getDetector(v)]->unassign(l);
			}
			removeFromTrail(var(l));
		}
		if(!opt_lazy_backtrack){
			assert(decisionLevel() <= lev);
			if(decisionLevel() > lev){
				throw std::runtime_error("Internal error in graph enqueue");
			}
		}

		if (g_under.outfile) {
			fprintf(g_under.outfile, "enqueue %d\n", dimacs(l));
			
			fprintf(g_under.outfile, "\n");
			fflush(g_under.outfile);
		}
		if (g_over.outfile) {
			fprintf(g_over.outfile, "enqueue %d\n", dimacs(l));
			fprintf(g_over.outfile, "\n");
			fflush(g_over.outfile);
		}

		assert(!onTrail(var(l)));
		appendToTrail(l,decisionLevel());
		requiresPropagation = true;

		assert(onTrail(var(l)));

		_assign(l);

	};

	void _assign(Lit l){
		assert(assigns[var(l)]==l_Undef);
		assigns[var(l)] = sign(l) ? l_False : l_True;
		if(hasTheory(var(l))){
			theories[getTheoryID(var(l))]->enqueueTheory(getTheoryLit(l));
			return;
		}

		if (isEdgeVar(var(l))) {
			
			//this is an edge assignment
			int edge_num = getEdgeID(var(l)); //v-min_edge_var;
			assert(edge_list[edge_num].v == var(l));
			
			int from = edge_list[edge_num].from;
			int to = edge_list[edge_num].to;
			if (!sign(l)) {
				g_under.enableEdge(edge_num);
			} else {
				g_over.disableEdge( edge_num);
				if (opt_conflict_min_cut) {//can optimize this by also checking if any installed detectors are actually using the cutgraph!
					assert(cutGraph.edgeEnabled(edge_num * 2 + 1));
					assert(!cutGraph.edgeEnabled(edge_num * 2));
					cutGraph.enableEdge( edge_num * 2);
					cutGraph.disableEdge( edge_num * 2 + 1);
				}
			}

			if(decisionLevel()==0){
				//assert(g_under.edgeEnabled(edge_num)== g_over.edgeEnabled(edge_num));
				g_under.makeEdgeAssignmentConstant(edge_num);
				g_over.makeEdgeAssignmentConstant(edge_num);
			}

			if(using_neg_weights){
				if (!sign(l)) {
					g_under_weights_over.enableEdge(edge_num);
				} else {
					g_over_weights_under.disableEdge(edge_num);
				}
				if(decisionLevel()==0){
					assert(g_under_weights_over.edgeEnabled(edge_num)== g_over_weights_under.edgeEnabled(edge_num));
					g_under_weights_over.makeEdgeAssignmentConstant(edge_num);
					g_over_weights_under.makeEdgeAssignmentConstant(edge_num);
				}
			}
		} else {
			//this is an assignment to a non-edge atom. (eg, a reachability assertion)
            int id = getDetector(var(l));
			detectors[id]->assign(l);
            if(detectors[id]->default_heuristic){
                activateHeuristic(detectors[id]->default_heuristic);
		}
		}
	}
    void activateHeuristic(Heuristic * h) override{
        S->activateHeuristic(h);
    }
/*
	bool backtrackWhileConflicting(Detector * d,vec<Lit> & conflict){
		conflict.clear();


		bool backtrackOnly = lazy_backtracking_enabled && lazy_trail_head!=var_Undef;
		if(lazy_trail_head==var_Undef){
			return d->propagate(conflict,backtrackOnly);
		}
		assert(lazy_trail_head!=var_Undef);
		Var v = getPrev(lazy_trail_head);

		bool r = false;

		while(v!=var_Undef && !r){
			Lit l = mkLit(v, value(v)==l_False);
			assert(assigns[v]!=l_Undef);
			backtrackAssign(l);
			Var p = _getPrev(v);
			requiresPropagation = true;
			if (removeFromTrail(v)){
				break;
			}
			r = d->propagate(conflict,lazy_trail_head!=var_Undef);
			if(r){
				//conflict backtrack'd past.
				return true;
			}
			v=p;
		}

		return d->propagate(conflict,lazy_trail_head!=var_Undef);
	}*/

	bool propagateTheory(vec<Lit> & conflict) {
		return propagateTheory(conflict,false);
	}
    Heuristic * conflictingHeuristic=nullptr;
    virtual Heuristic * getConflictingHeuristic()override{
        return conflictingHeuristic;
    }


	bool propagateTheory(vec<Lit> & conflict, bool force_propagation) {
		dbg_check_trails();
        conflictingHeuristic=this;
		if(theoryIsSatisfied()){
			S->setTheorySatisfied(this);
			if(bvTheory){
				bvTheory->setTheorySatisfied(this);
			}
			stats_propagations_skipped++;
			return true;
		}
/*#ifdef DEBUG_GRAPH
		for(int v = 0;v<nVars();v++){
			if(v==var(const_true))
				continue;
			Var solverVar = toSolver(v);
			if(S->hasTheory(solverVar)) {
				lbool val = value(v);
				lbool solverVal = S->value(solverVar);
				if (val != solverVal) {
					exit(2);
				}
			}
		}
#endif*/


		static int itp = 0;
		if (++itp == 4) {
			int a = 1;
		}
		dbg_graphsUpToDate();
		stats_propagations++;

		if (!force_propagation && !requiresPropagation) {
			dbg_sync();
			stats_propagations_skipped++;
			assert(dbg_graphsUpToDate());
			dbg_graphsUpToDate();
			return true;
		}


		propagations++;

		if (!force_propagation && (propagations % opt_graph_prop_skip != 0)){
			stats_propagations_skipped++;

			return true;
		}

		for(Theory * t:propagation_required_theories){
			if(!t->propagateTheory(conflict)) {
                conflictingHeuristic=t->getConflictingHeuristic();
			return false;
		}
		}
		S->theoryPropagated(this);

		if (opt_lazy_backtrack &&
				!lazy_backtracking_enabled && decisionLevel()==0 ){
			lazy_backtracking_enabled=true;
			//currently, lazy backtracking is only supported if _all_ property lits are ground.
			for (Detector * d:detectors){
				if(d->unassigned_negatives >0 && d->unassigned_positives>0){
					//at least one property lit of this detector is unassigned, so disable lazy_backtracking.
					lazy_backtracking_enabled=false;
					break;
				}
			}
		}


		dbg_sync();
		conflict.clear();
		//printf("graph prop %d\n",stats_propagations);
		for(Theory * t:theories){
			if(!t->propagateTheory(conflict)){
                conflictingHeuristic=t->getConflictingHeuristic();

				return false;
			}
		}
		bool any_change = false;
		double startproptime = rtime(1);
		
		conflict.clear();
		//Can probably speed this up alot by a) constant propagating reaches that I care about at level 0, and b) Removing all detectors for nodes that appear only in the opposite polarity (or not at all) in the cnf.
		//That second one especially.
		
		//At level 0, need to propagate constant reaches/source nodes/edges...

/*		for (int bvID:bvs_to_update){
			assert(bv_needs_update[bvID]);
			if(isBVEdge(bvID)){
				int edgeID = getBVEdge(bvID);
				g_under.setEdgeWeight(edgeID,edge_bv_weights[bvID].getUnder());
				g_over.setEdgeWeight(edgeID, edge_bv_weights[bvID].getOver());
				if(using_neg_weights){
					g_under.setEdgeWeight(edgeID,edge_bv_weights[bvID].getOver());
					g_over.setEdgeWeight(edgeID, edge_bv_weights[bvID].getUnder());
				}
				bv_needs_update[bvID]=false;
			}
		}
		bvs_to_update.clear();*/

		//dbg_sync();
		assert(dbg_graphsUpToDate());

		for (int d = 0; d < detectors.size(); d++) {
				if(satisfied_detectors[d])
					continue;
			assert(conflict.size() == 0);
			Lit l = lit_Undef;
			bool backtrackOnly = lazy_backtracking_enabled && (opt_lazy_conflicts==3) &&  lazy_trail_head!=var_Undef;
			bool r = detectors[d]->propagate(conflict,backtrackOnly,l);
			if(!r && backtrackOnly && conflict.size()==0){
				backtrackUntil(decisionLevel());
				stats_num_lazy_conflicts++;
				d=-1;
				continue;
			}

			if (!r) {


				if(conflict.size() && lazy_backtracking_enabled && lazy_trail_head!=var_Undef){
					//find the highest level lit in the conflict; if it is a higher level than the SAT solver, then this isn't a conflict in the SAT solver (though the learnt clause is a valid one)

					//There are several options for how to deal with this:

					//0) Completely sync up the theory solver with the SAT solver at this point, and re-propagate
					//1) add the clause to the SAT solver, and simply unassign the lit in the theory solver.
					//2) Unassign _all_ lazy lits from the conflict, and repropagate.
					//3) flip the assignment of one of the lits _in the theory solver_; propagate again.
					//(it might also be a good idea to add that assignment as a decision in the SAT solver.)

					//if lits are unassigned, we may also want to put those at the head of the decision heuristic (to make the sat solver revisit them).



					//assert(!seen.contains(true));
					//seen.growTo(vars.size());

					bool any_seen=false;
					if(opt_lazy_conflicts==0){
						for (Lit l:conflict){
							if(S->value(toSolver(l))!=value(l)){
								any_seen=true;
								assert(onLazyTrail(var(l)));
								break;
							}
						}
						if(any_seen){
							//sync the solver:
							backtrackUntil(decisionLevel());
							assert(lazy_trail_head==var_Undef);
							if(opt_lazy_backtrack_redecide){
								//redecide the lits of the conflict
								for (Lit l:conflict){
									if(isEdgeVar(var(l)) && S->value(var(toSolver(l)))==l_Undef){
										for (int i = 0; i < detectors.size(); i++) {
											Detector * r = detectors[i];
											r->suggestDecision(l);
										}
									}
								}
							}

						}
					}else if(opt_lazy_conflicts==1){
						for (Lit l:conflict){
							if(S->value(toSolver(l))!=value(l)){
								any_seen=true;
								assert(onLazyTrail(var(l)));

								removeFromTrail(var(l));
								backtrackAssign(~l);
								if(opt_lazy_backtrack_redecide && isEdgeVar(var(l))){
									for (int i = 0; i < detectors.size(); i++) {
										Detector * r = detectors[i];
										r->suggestDecision(l);
									}
								}

							}
						}
					}else if(opt_lazy_conflicts==2){
						for (Lit l:conflict){
							if(S->value(toSolver(l))!=value(l)){

								any_seen=true;
								assert(onLazyTrail(var(l)));

								removeFromTrail(var(l));
								backtrackAssign(~l);
								if(opt_lazy_backtrack_redecide && isEdgeVar(var(l))){
									for (int i = 0; i < detectors.size(); i++) {
										Detector * r = detectors[i];
										r->suggestDecision(l);
									}
								}
								break;
							}
						}
					}
					if(any_seen){

						//the conflict should now be eliminated.
						if(opt_keep_lazy_conflicts){

							S->addClauseSafely(conflict);
						}
						stats_num_lazy_conflicts++;
						conflict.clear();
						//restart the loop, as assignments have changed... actually, this shouldn't be neccesary (only the current propagation should be re-started)
						//as we have not backtracked past the current level in the SAT solver.
						d=-1;
						continue;
					}
				}
				stats_num_conflicts++;
                    conflictingHeuristic=detectors[d]->getConflictingHeuristic();
				propagationtime += rtime(1) - startproptime;
				return false;
			}
		}

		dbg_full_sync();
		
		requiresPropagation = false;
		g_under.clearChanged();
		g_over.clearChanged();
		cutGraph.clearChanged();
		
		g_under.clearHistory();
		g_over.clearHistory();
		cutGraph.clearHistory();

		g_under_weights_over.clearChanged();
		g_over_weights_under.clearChanged();
		g_under_weights_over.clearHistory();
		g_over_weights_under.clearHistory();
		dbg_graphsUpToDate();
		double elapsed = rtime(1) - startproptime;
		propagationtime += elapsed;
		dbg_sync();
		dbg_sync_reachability();
		dbg_check_trails();

		if(theoryIsSatisfied()){
			S->setTheorySatisfied(this);
			if(bvTheory){
				bvTheory->setTheorySatisfied(this);
			}
		}

		return true;
	}

	bool supportsLazyBacktracking(){
		return lazy_backtracking_enabled;
	}

	bool solveTheory(vec<Lit> & conflict) {
		n_theory_solves++;
		requiresPropagation = true;		//Just to be on the safe side... but this shouldn't really be required.
		bool ret = propagateTheory(conflict,true);
		if(ret){
			for (Detector *d:detectors)
				d->buildModel();
		}
		//Under normal conditions, this should _always_ hold (as propagateTheory should have been called and checked by the parent solver before getting to this point).
		assert(ret);
		return ret;
	}
	;

	void drawFull(int from, int to) {
		printf("digraph{\n");
		for (int i = 0; i < nNodes(); i++) {
			if (i == from) {
				printf("n%d [label=\"From\", style=filled, fillcolor=blue]\n", i);
			} else if (i == to) {
				printf("n%d [label=\"To\", style=filled, fillcolor=red]\n", i);
			} else
				printf("n%d\n", i);
		}
		
		for (int i = 0; i < edge_list.size(); i++) {
			if (edge_list[i].v < 0)
				continue;
			Edge & e = edge_list[i];
			const char * s = "black";
			if (value(e.v) == l_True)
				s = "blue";
			else if (value(e.v) == l_False)
				s = "red";
			printf("n%d -> n%d [label=\"v%d\",color=\"%s\"]\n", e.from, e.to, e.v, s);
		}
		
		printf("}\n");
	}
	void drawFull(bool forceDraw=false, bool drawDisabled=true) {

#ifndef DEBUG_GRAPH
		if(!forceDraw)
			return;
#endif
		printf("digraph{\n");
		for (int i = 0; i < nNodes(); i++) {
			if(getNodeName(i)){
				printf("n%d[label=\"n%d_%s\"]\n",i,i,getNodeName(i));
			}else{
			printf("n%d\n", i);
		}
		}

		for (int i = 0; i < edge_list.size(); i++) {
			Edge & e = edge_list[i];
			const char * s = "black";
			if (value(e.v) == l_True)
				s = "blue";
			else if (value(e.v) == l_False){
				if(!drawDisabled)
					continue;
				s = "red";
			}else {
				int a = 1;
			}
			if(getEdgeName(e.v)){
				printf("n%d -> n%d [label=\"v%d_%s\",color=\"%s\"]\n", e.from, e.to, e.v,getEdgeName(e.v), s);
			}else{
			printf("n%d -> n%d [label=\"v%d\",color=\"%s\"]\n", e.from, e.to, e.v, s);
		}
		}

		printf("}\n");

	}
	
	bool check_solved() {
		if (opt_print_graph) {
			drawFull();
		}
		dbg_graphsUpToDate();
		for (int i = 0; i < edge_list.size(); i++) {
			if (edge_list[i].v < 0)
				continue;
			Edge & e = edge_list[i];
			lbool val = value(e.v);
			//this can be allowed
			/*if (val == l_Undef) {
				throw std::runtime_error("BAD SOLUTION: Unassigned edge!");
				return false;
			}*/
			
			if (val == l_True) {
				/*	if(!g.hasEdge(e.from,e.to)){
				 return false;
				 }
				 if(!antig.hasEdge(e.from,e.to)){
				 return false;
				 }*/
				if (!g_under.edgeEnabled(e.edgeID)) {
					throw std::runtime_error("BAD SOLUTION: missing true edge in g_under");
					return false;
				}
				if (!g_over.edgeEnabled(e.edgeID)) {
					throw std::runtime_error("BAD SOLUTION: missing true edge in g_over");
					return false;
				}
				if(edge_bv_weights.size()){
					BitVector<Weight> & bv = edge_bv_weights[e.edgeID];
					if(g_under.getWeight(e.edgeID)!= bv.getUnder()){
						throw std::runtime_error("BAD SOLUTION: bad edge weight in g_under");
						return false;
					}
					if(g_over.getWeight(e.edgeID)!=bv.getOver()){
						throw std::runtime_error("BAD SOLUTION: bad edge weight in g_over");
						return false;
					}
				}
			} else if(val==l_False) {
				/*if(g.hasEdge(e.from,e.to)){
				 return false;
				 }*/
				if (g_under.edgeEnabled(e.edgeID)) {
					throw std::runtime_error("BAD SOLUTION: included false edge in g_under");
					return false;
				}
				if (g_over.edgeEnabled(e.edgeID)) {
						throw std::runtime_error("BAD SOLUTION: included false edge in g_over");
					return false;
				}
				/*if(antig.hasEdge(e.from,e.to)){
				 return false;
				 }*/

			}
			if(using_neg_weights){

				if (val == l_True) {
					/*	if(!g.hasEdge(e.from,e.to)){
					 return false;
					 }
					 if(!antig.hasEdge(e.from,e.to)){
					 return false;
					 }*/
					if (!g_under_weights_over.edgeEnabled(e.edgeID)) {
						return false;
					}
					if (!g_over_weights_under.edgeEnabled(e.edgeID)) {
						return false;
					}
					if(edge_bv_weights.size()){
						BitVector<Weight> & bv = edge_bv_weights[e.edgeID];
						if(g_under_weights_over.getWeight(e.edgeID)!=bv.getOver()){
							return false;
						}
						if(g_over_weights_under.getWeight(e.edgeID)!=bv.getUnder()){
							return false;
						}
					}
				} else {
					/*if(g.hasEdge(e.from,e.to)){
					 return false;
					 }*/
					if (g_under_weights_over.edgeEnabled(e.edgeID)) {
						return false;
					}
					if (g_over_weights_under.edgeEnabled(e.edgeID)) {
						return false;
					}
					/*if(antig.hasEdge(e.from,e.to)){
					 return false;
					 }*/

				}

			}
		}
		for (int i = 0; i < detectors.size(); i++) {
			if (!detectors[i]->checkSatisfied()) {
				printf("Error in solution of graph theory %d, in detector %d (%s)\n", this->getTheoryIndex(), i, detectors[i]->getName());
				throw std::runtime_error("BAD SOLUTION: failure in graph theory detector");
				return false;
			}
		}
		return true;
	}
	
	bool dbg_solved() {
#ifdef DEBUG_GRAPH
		for(int i = 0;i<edge_list.size();i++) {
			if(edge_list[i].v<0)
			continue;
			Edge & e = edge_list[i];
			lbool val = value(e.v);
			assert(val!=l_Undef);

			if(val==l_True) {
				assert(g_under.hasEdge(e.from,e.to));
				assert(g_over.hasEdge(e.from,e.to));
			} else {
				assert(!g_under.hasEdge(e.from,e.to));
				assert(!g_over.hasEdge(e.from,e.to));
			}

		}
		assert(check_solved());
		/*for(int i = 0;i<reach_detectors.size();i++){
		 ReachDetector* d  = reach_detectors[i];
		 for(int j = 0;j< d->reach_lits.size();j++){
		 Lit l = d->reach_lits[j];
		 if(l!=lit_Undef){
		 int node = d->getNode(var(l));

		 if(value(l)==l_True){
		 assert(d->positive_reach_detector->connected(node));
		 }else if (value(l)==l_False){
		 assert(! d->negative_reach_detector->connected(node));
		 }else{
		 assert(!d->positive_reach_detector->connected(node));
		 assert(d->negative_reach_detector->connected(node));
		 }
		 }
		 }
		 }*/
#endif
		return true;
	}
	
	void drawCurrent() {
#ifdef DEBUG_GRAPH
		int from = -1;
		int to = -1;
		printf("digraph{\n");
		for (int i = 0; i < nNodes(); i++) {
			if (i == from) {
				printf("n%d [label=\"From\", style=filled, fillcolor=blue]\n", i);
			} else if (i == to) {
				printf("n%d [label=\"To\", style=filled, fillcolor=red]\n", i);
			} else
				printf("n%d\n", i);
		}

		for (int i = 0; i < edge_list.size(); i++) {
			if (edge_list[i].v < 0)
				continue;
			Edge & e = edge_list[i];
			const char * s = "black";
			if (value(e.v) == l_True)
				s = "blue";
			else if (value(e.v) == l_False)
				continue;
				//s = "red";
			printf("n%d -> n%d [label=\"v%d\",color=\"%s\"]\n", e.from, e.to, e.v, s);
		}
		
		printf("}\n");
#endif
	}
	int nEdges() {
		return edge_list.size();
	}
	CRef newReasonMarker(int detectorID) {
		CRef reasonMarker = S->newReasonMarker(this);
		int mnum = CRef_Undef - reasonMarker;
		marker_map.growTo(mnum + 1);
		marker_map[mnum].forTheory=false;
		marker_map[mnum].id = detectorID;
		return reasonMarker;
	}

    void addHeuristic(Heuristic * h){
        S->addHeuristic(h);
    }
    Solver * getSolver(){
        return S;
    }
private:
	vec<vec<int> > & getComparisonSet(Comparison op){
		switch(op){
			case Comparison::lt:
				return comparisons_lt;
			case Comparison::leq:
				return comparisons_leq;
			case Comparison::gt:
				return comparisons_gt;
			case Comparison::geq:
			default:
				return comparisons_geq;
		}
	}
	Lit getComparison(int bvID,Comparison op,const Weight & w){
		//could do a binary search here:

		vec<vec<int> > & comparison = getComparisonSet(op);
		comparison.growTo(bvID+1);
		for(int i=0;i<comparison[bvID].size()-1;i++){
			int cID = comparison[bvID][i];
			if (comparisons[cID].w == w){
				return comparisons[cID].l;
			}
		}
		return lit_Undef;
	}
public:


	Lit getBV_COMP(int bvID,Comparison op,const Weight & w){

			assert(bvID>=0);
			//if has existing literal, we really shouldn't create a new one here...
			Lit l=lit_Undef;
			if((l=getComparison(bvID,op,w))!=lit_Undef){
				return l;
			}
			int comparisonID = comparisons.size();
			Lit gt = bvTheory->newComparison(op,bvID,w,var_Undef,opt_cmp_lits_decidable);
			l = mkLit(newVar());

			makeEqualInSolver(bvTheory->toSolver(gt),toSolver(l));

			comparisons.push({w,l,bvID,true});


			vec<vec<int> > & comparison = getComparisonSet(op);
			//comparisons_gt[bvID].push(comparisonID);
			//insert this value in order.
			//could do a binary search here...
			for(int i=0;i<comparison[bvID].size()-1;i++){
				int cid = comparison[bvID][i];
				if(comparisons[cid].w>= w){
					for(int j = comparison[bvID].size()-1; j>i ;j--){
						comparison[bvID][j]=comparison[bvID][j-1];
					}
					comparison[bvID][i]=comparisonID;
					break;
				}
			}

			return l;
		}


	Lit getEdgeWeightGT(int edgeID,const Weight & w){
		if(edge_bv_weights.size()>edgeID){
			int bvID = edge_bv_weights[edgeID].getID();
			return getBV_COMP(bvID,Comparison::gt,w);
		}else{
			return mkLit(getEdgeVar(edgeID));
		}
	}



	Lit getEdgeWeightGEQ(int edgeID,const Weight w){
		if(edge_bv_weights.size()>edgeID){
			int bvID = edge_bv_weights[edgeID].getID();
			return getBV_COMP(bvID,Comparison::geq,w);
		}else{
			return mkLit(getEdgeVar(edgeID));
		}
	}

	//True if the weight of this edge is constant.
	bool constantWeight(int edgeID) const{
		//improve this, later...
		return !( edge_bv_weights.size()>edgeID);
	}

	Lit getEdgeWeightLT(int edgeID,const Weight & w){
		if(edge_bv_weights.size()>edgeID){
			int bvID = edge_bv_weights[edgeID].getID();
			return getBV_COMP(bvID,Comparison::lt,w);
		}else{
			return mkLit(getEdgeVar(edgeID));
		}
	}


	Lit getEdgeWeightLEQ(int edgeID,const Weight & w){
			if(edge_bv_weights.size()>edgeID){
				int bvID = edge_bv_weights[edgeID].getID();
				return getBV_COMP(bvID,Comparison::leq,w);
			}else{
				return mkLit(getEdgeVar(edgeID));
			}
		}
/*	int getEdgeBV(int edgeID){
		assert(edge_bv_weights.size()>edgeID);
		return edge_bv_weights[edgeID].getID();
	}*/

	bool isEdgeBV(int bvID){
		return bvID<bitvector_data.size() && bitvector_data[bvID].edgeID>=0;
	}
	int getBVEdge(int bvID){
		assert(isEdgeBV(bvID));
		return bitvector_data[bvID].edgeID;
	}

	bool isBVVar(Var v){
		return vars[v].isBV;
	}

	int getBVForVar(Var v){
		assert(isBVVar(v));
		return vars[v].detector_edge;
	}
	int getEdgeWeightBitWidth(){
		if(edge_bv_weights.size()>0){
			return edge_bv_weights[0].width();
		}else{
			assert(false);
			return -1;
		}
	}
	//Create a fresh bitvector
	BitVector<Weight> newBV(Weight constval=-1,int bitwidth=-1) {
		if (bitwidth<0)
			bitwidth = getEdgeWeightBitWidth();
		BitVector<Weight> bv = bvTheory->newBitvector(-1,bitwidth,constval);
		return bv;
	}

	Lit newEdgeBV(int from, int to, Var outerVar,vec<Var> & bitVector) {
		if(!bvTheory ){
			fprintf(stderr,"No bitvector theory initialized\n");exit(1);
		}
		while(from>=nNodes()||to>=nNodes())
			newNode();
		has_any_bitvector_edges=true;
			assert(outerVar!=var_Undef);
			assert(edge_weights.size()==0);
			/*	if(outerVar==var_Undef)
			 outerVar = S->newVar();*/
			vec<Var> internalBV;
			int bvID = bitvector_data.size();
			int index = edge_list.size();
			for (Var v:bitVector){
				internalBV.push(newBVVar(v,bvID,index));
			}

		/*	if(!bvTheory){
				bvTheory = new BVTheorySolver<Weight,ComparisonStatus>(ComparisonStatus(*this, *bvTheory));
			}*/
			BitVector<Weight> bv = bvTheory->newBitvector(bvID,bitVector);
			bvTheory->setBitvectorTheory(bvID,this->getTheoryIndex());
			bitvector_data.growTo(bv.getID()+1);
			if(bitvector_data[bv.getID()].edgeID!=-1 || bitvector_data[bv.getID()].detectorID!=-1){
				//else, this is already used!
				fprintf(stderr,"Each bitvector can only be used for one edge!\n");
				exit(1);
			}
			bitvector_data[bv.getID()].edgeID=index;
			//bv_needs_update.growTo(bv.getID()+1);
			all_edges_unit &= (bv.getUnder()== 1 && bv.getOver()==1);
			all_edges_positive &= bv.getUnder()>0;
			edge_list.push();
			Var v = newVar(outerVar, index, true);
			comparisons_lt.growTo(bv.getID()+1);
			comparisons_gt.growTo(bv.getID()+1);
			comparisons_leq.growTo(bv.getID()+1);
			comparisons_geq.growTo(bv.getID()+1);
			undirected_adj[to].push( { v, outerVar, from, to, index });
			undirected_adj[from].push( { v, outerVar, to, from, index });
			inv_adj[to].push( { v, outerVar, from, to, index });

			//num_edges++;
			edge_list[index].v = v;
			edge_list[index].outerVar = outerVar;
			edge_list[index].from = from;
			edge_list[index].to = to;
			edge_list[index].edgeID = index;
			edge_list[index].bvID = bv.getID();

			if (edge_bv_weights.size() <= index) {
				edge_bv_weights.resize(index + 1);
			}
			edge_bv_weights[index] = bv;

			//edges[from][to]= {v,outerVar,from,to,index};
			g_under.addEdge(from, to, index,bv.getUnder());
			g_under.disableEdge(from, to, index);
			g_over.addEdge(from, to, index,bv.getOver());


			g_under_weights_over.addEdge(from, to, index,bv.getOver());
			g_under_weights_over.disableEdge(from, to, index);
			g_over_weights_under.addEdge(from, to, index,bv.getUnder());


			cutGraph.addEdge(from, to, index * 2,1);
			cutGraph.addEdge(from, to, index * 2 + 1,0xFFFF);
			cutGraph.disableEdge(from, to, index * 2);


			if (g_under.outfile) {

				fprintf(g_under.outfile, "edge_bv_weight %d", index);
				for(Var v:bitVector)
					fprintf(g_under.outfile," %d", v+1);
				fprintf(g_under.outfile,"\n");
				fflush(g_under.outfile);
			}
			if (g_over.outfile) {
				fprintf(g_over.outfile, "edge_bv_weight %d", index);
				for(Var v:bitVector)
					fprintf(g_over.outfile," %d", v+1);
				fprintf(g_over.outfile,"\n");
				fflush(g_over.outfile);
			}
			if (cutGraph.outfile) {

			//	fprintf(cutGraph.outfile, "edge_weight %d %d\n", index * 2, 1);
			//	fprintf(cutGraph.outfile, "edge_weight %d %d\n", index * 2 + 1, 0xFFFF);
				fflush(cutGraph.outfile);
			}

			return mkLit(v, false);
		}
	Lit newEdgeBV(int from, int to, Var outerVar,int bvID) {
		if(!bvTheory){
			fprintf(stderr,"No bitvector theory initialized\n");exit(1);
		}
		if(!bvTheory->hasBV(bvID)){
			fprintf(stderr,"Undefined bitvector\n");exit(1);
		}
		if(bvTheory->hasTheory(bvID)){
			bvID = bvTheory->duplicateBV(bvID).getID();
		}
		if(isEdgeBV(bvID)){
			fprintf(stderr,"Bitvector %d used for multiple edge weights\n", bvID);exit(1);
		}
		while(from>=nNodes()||to>=nNodes())
			newNode();
		has_any_bitvector_edges=true;
		assert(outerVar!=var_Undef);
		assert(edge_weights.size()==0);
		int index = edge_list.size();
		bvTheory->setBitvectorTheory(bvID,this->getTheoryIndex());
		BitVector<Weight> bv = bvTheory->getBV(bvID);
		bitvector_data.growTo(bv.getID()+1);
		bitvector_data[bv.getID()].edgeID=index;
		//bvTheory->setCallback(bv.getID(),&bvcallback);
		comparisons_lt.growTo(bv.getID()+1);
		comparisons_gt.growTo(bv.getID()+1);
		comparisons_leq.growTo(bv.getID()+1);
		comparisons_geq.growTo(bv.getID()+1);

		all_edges_unit &= (bv.getUnder()== 1 && bv.getOver()==1);
		all_edges_positive &= bv.getUnder()>0;
		edge_list.push();
		Var v = newVar(outerVar, index, true);
		//bv_needs_update.growTo(bv.getID()+1);
		undirected_adj[to].push( { v, outerVar, from, to, index });
		undirected_adj[from].push( { v, outerVar, to, from, index });
		inv_adj[to].push( { v, outerVar, from, to, index });

		//num_edges++;
		edge_list[index].v = v;
		edge_list[index].outerVar = outerVar;
		edge_list[index].from = from;
		edge_list[index].to = to;
		edge_list[index].edgeID = index;
		edge_list[index].bvID = bv.getID();

		if (edge_bv_weights.size() <= index) {
			edge_bv_weights.resize(index + 1);
		}
		edge_bv_weights[index] = bv;

		//edges[from][to]= {v,outerVar,from,to,index};
		g_under.addEdge(from, to, index,bv.getUnder());
		g_under.disableEdge(from, to, index);
		g_over.addEdge(from, to, index,bv.getOver());


		g_under_weights_over.addEdge(from, to, index,bv.getOver());
		g_under_weights_over.disableEdge(from, to, index);
		g_over_weights_under.addEdge(from, to, index,bv.getUnder());


		cutGraph.addEdge(from, to, index * 2,1);
		cutGraph.addEdge(from, to, index * 2 + 1,0xFFFF);
		cutGraph.disableEdge(from, to, index * 2);

				return mkLit(v, false);
			}
	Lit newEdge(int from, int to, Var outerVar = var_Undef, Weight weight = 1) {
		assert(outerVar!=var_Undef);
		while(from>=nNodes()||to>=nNodes())
			newNode();

		/*	if(outerVar==var_Undef)
		 outerVar = S->newVar();*/
		assert(bitvector_data.size()==0);
		all_edges_unit &= (weight == 1);
		all_edges_positive &= weight>0;
		int index = edge_list.size();
		edge_list.push();
		Var v = newVar(outerVar, index, true);

		/*
		 if(num_edges>0){
		 }else
		 min_edge_var=v;

		 int index = v-min_edge_var;*/
		/*
		 while(edge_list.size()<=index){
		 edge_list.push({-1,-1,-1,-1,-1,1});
		 assigns.push(l_Undef);
		 }*/
		undirected_adj[to].push( { v, outerVar, from, to, index });
		undirected_adj[from].push( { v, outerVar, to, from, index });
		inv_adj[to].push( { v, outerVar, from, to, index });
		
		//num_edges++;
		edge_list[index].v = v;
		edge_list[index].outerVar = outerVar;
		edge_list[index].from = from;
		edge_list[index].to = to;
		edge_list[index].edgeID = index;
		//edge_list[index].weight=weight;
		if (edge_weights.size() <= index) {
			edge_weights.resize(index + 1);
		}
		edge_weights[index] = weight;
		
		//edges[from][to]= {v,outerVar,from,to,index};
		g_under.addEdge(from, to, index,weight);
		g_under.disableEdge(from, to, index);
		g_over.addEdge(from, to, index,weight);


		g_under_weights_over.addEdge(from, to, index,weight);
		g_under_weights_over.disableEdge(from, to, index);
		g_over_weights_under.addEdge(from, to, index,weight);


		cutGraph.addEdge(from, to, index * 2,1);
		cutGraph.addEdge(from, to, index * 2 + 1,0xFFFF);
		cutGraph.disableEdge(from, to, index * 2);
		



		return mkLit(v, false);
	}
	/*	int getEdgeID(int from, int to){
	 assert(edges[from][to].edgeID>=0);
	 return edges[from][to].edgeID;
	 }*/
/*	Weight getWeight(int edgeID) {
		return edge_weights[edgeID];
	}*/
	void reachesWithinSteps(int from, int to, Var reach_var, int within_steps) {
		while(from >= g_under.nodes() || to >= g_under.nodes()){
			newNode();
		}
		if(to >= g_under.nodes()){
			fprintf(stderr, "Undefined node %d\n",to);
			exit(1);
		}
		if(from >= g_under.nodes()){
			fprintf(stderr, "Undefined node %d\n",to);
			exit(1);
		}
		if (within_steps <= -1)
			within_steps = g_under.nodes();
		
		if (dist_info[from].source < 0) {
			DistanceDetector<Weight> * d = new DistanceDetector<Weight>(detectors.size(), this,  g_under, g_over,
					from, drand(rnd_seed));

			addDetector(d);

			distance_detectors.push(d);
			assert(detectors.last()->getID() == detectors.size() - 1);
			dist_info[from].source = from;
			dist_info[from].detector = detectors.last();
		}
		
		DistanceDetector<Weight> * d = (DistanceDetector<Weight>*) dist_info[from].detector;
		assert(d);
		
		d->addUnweightedShortestPathLit(from, to, reach_var, within_steps);
		
	}

	void enableNegativeWeights(){
		if(!using_neg_weights){
			using_neg_weights=true;

			for(int i = 0;i<edge_list.size();i++) {
				if(edge_list[i].v<0)
				continue;
				Edge e = edge_list[i];
				lbool val = value(e.v);
				g_under_weights_over.setEdgeEnabled(i, (g_under.edgeEnabled(i)));
				g_over_weights_under.setEdgeEnabled(i, (g_over.edgeEnabled(i)));
				g_under_weights_over.setEdgeWeight(i,g_over.getWeight(i) );
				g_over_weights_under.setEdgeWeight(i, g_under.getWeight(i));
			}
		}
	}

	void reachesWithinDistance(int from, int to, Var reach_var, Weight distance, bool strictComparison) {
		if(to >= g_under.nodes()){
			fprintf(stderr, "Undefined node %d\n",to);
			exit(1);
		}
		if(from >= g_under.nodes()){
			fprintf(stderr, "Undefined node %d\n",to);
			exit(1);
		}
		
		while(from >= g_under.nodes() || to >= g_under.nodes()){
			newNode();
		}
		if (weighted_dist_info[from].source < 0) {
			WeightedDistanceDetector<Weight> * d;
			if(edge_bv_weights.size()>0){
				enableNegativeWeights();
				d = new WeightedDistanceDetector<Weight>(detectors.size(), this,  g_under_weights_over, g_over_weights_under,
						from, drand(rnd_seed));
			}else{
				d = new WeightedDistanceDetector<Weight>(detectors.size(), this,  g_under, g_over,
						from, drand(rnd_seed));
			}
			addDetector(d);

			weighted_distance_detectors.push(d);
			assert(detectors.last()->getID() == detectors.size() - 1);
			weighted_dist_info[from].source = from;
			weighted_dist_info[from].detector = detectors.last();
		}

		WeightedDistanceDetector<Weight> * d = (WeightedDistanceDetector<Weight>*) weighted_dist_info[from].detector;
		assert(d);
		
		d->addWeightedShortestPathLit(from, to, reach_var, distance,strictComparison);
		
	}

	void reachesWithinDistanceBV(int from, int to, Var reach_var, int bvID, bool strictComparison) {
		while(from >= g_under.nodes() || to >= g_under.nodes()){
			newNode();
		}
		if(to >= g_under.nodes()){
			fprintf(stderr, "Undefined node %d\n",to);
			exit(1);
		}
		if(from >= g_under.nodes()){
			fprintf(stderr, "Undefined node %d\n",to);
			exit(1);
		}
		if(!bvTheory){
			fprintf(stderr,"No bitvector theory initialized\n");exit(1);
		}
		if(!bvTheory->hasBV(bvID)){
			fprintf(stderr,"Undefined bitvector\n");exit(1);
		}
		bvTheory->setBitvectorTheory(bvID,this->getTheoryIndex());
		assert(from < g_under.nodes());

		if (weighted_dist_info[from].source < 0) {
			WeightedDistanceDetector<Weight> * d;
			if(edge_bv_weights.size()>0){
				enableNegativeWeights();
				d = new WeightedDistanceDetector<Weight>(detectors.size(), this,  g_under_weights_over, g_over_weights_under,
						from, drand(rnd_seed));
			}else{
				d = new WeightedDistanceDetector<Weight>(detectors.size(), this,  g_under, g_over,
						from, drand(rnd_seed));
			}
			addDetector(d);

			weighted_distance_detectors.push(d);
			assert(detectors.last()->getID() == detectors.size() - 1);
			weighted_dist_info[from].source = from;
			weighted_dist_info[from].detector = detectors.last();
		}

		WeightedDistanceDetector<Weight> * d = (WeightedDistanceDetector<Weight>*) weighted_dist_info[from].detector;
		assert(d);
		BitVector<Weight> bv = bvTheory->getBV(bvID);
		d->addWeightedShortestPathBVLit(from, to, reach_var,bv,strictComparison );

	}

	void implementMaxflowBV(int from, int to, Var v, int bvID, bool strictComparison) {
		while(from >= g_under.nodes() || to >= g_under.nodes()){
			newNode();
		}

		if(from==to){
			//The maxflow from a node to itself is always infinite, and so that flow is never less than any specific weight
			if(v!=var_Undef) {
				S->addClause(~mkLit(v));
			}
			return;
		}
		if(!bvTheory){
			fprintf(stderr,"No bitvector theory initialized\n");exit(1);
		}
		if(!bvTheory->hasBV(bvID)){
			fprintf(stderr,"Undefined bitvector\n");exit(1);
		}
		bvTheory->setBitvectorTheory(bvID,this->getTheoryIndex());
		for (int i = 0; i < flow_detectors.size(); i++) {
			if (flow_detectors[i]->source == from && flow_detectors[i]->target == to) {
				flow_detectors[i]->addMaxFlowGEQ_BV(bvTheory->getBV(bvID), v,!strictComparison);
				return;
			}
		}
		MaxflowDetector<Weight> *f = new MaxflowDetector<Weight>(detectors.size(), this,  g_under, g_over, from,
				to, drand(rnd_seed));
		flow_detectors.push(f);
		int detectorID = detectors.size();
		addDetector(f);
		//setBVDetectorID(bvID, detectorID);
		f->addMaxFlowGEQ_BV(bvTheory->getBV(bvID), v, !strictComparison);

	}

	void implementConstraints() {
		if (!S->okay())
			return;
		if (opt_allpairs_percentage >= 1) {
			for (int i = 0; i < unimplemented_reachability_constraints.size(); i++) {
				ReachabilityConstraint c = unimplemented_reachability_constraints[i];
				reaches_private(c.from, c.to, c.reach_var, c.distance);
			}
			
		} else if (opt_allpairs_percentage == 0) {
			for (int i = 0; i < unimplemented_reachability_constraints.size(); i++) {
				ReachabilityConstraint c = unimplemented_reachability_constraints[i];
				allpairs(c.from, c.to, c.reach_var, c.distance);
			}
		} else {
			{
				vec<bool> seen;
				int count = 0;
				seen.growTo(nNodes());
				for (int i = 0; i < unimplemented_reachability_constraints.size(); i++) {
					ReachabilityConstraint c = unimplemented_reachability_constraints[i];
					if (!seen[c.from]) {
						seen[c.from] = true;
						count++;
					}
				}
				double frac = ((double) count) / ((double) nNodes());
				
				if (opt_verb > 0 && frac >= opt_allpairs_percentage) {
					printf("Allpairs solver triggered for graph %d by percentage of source nodes: %d/%d=%f>%f\n",
							getGraphID(), count, nNodes(), frac, (double) opt_allpairs_percentage);
				}
				
				for (int i = 0; i < unimplemented_reachability_constraints.size(); i++) {
					ReachabilityConstraint c = unimplemented_reachability_constraints[i];
					if (frac >= opt_allpairs_percentage)
						allpairs(c.from, c.to, c.reach_var, c.distance);
					else
						reaches_private(c.from, c.to, c.reach_var, c.distance);
				}
			}
		}
		unimplemented_reachability_constraints.clear();
		
		for(auto & d:unimplemented_distance_constraints){
			reachesWithinDistance(d.from, d.to, d.reach_var, d.distance,d.strict);
		}
		unimplemented_distance_constraints.clear();

		for(auto & d:unimplemented_distance_constraints_bv){
			reachesWithinDistanceBV(d.from, d.to, d.var, d.bvID,d.strict);
		}
		unimplemented_distance_constraints_bv.clear();

		for(auto & d:unimplemented_maxflow_constraints_bv){
			implementMaxflowBV(d.s, d.t, d.var, d.bvID,d.strict);
		}
		unimplemented_maxflow_constraints_bv.clear();
	}
	void allpairs_undirected(int from, int to, Var reach_var, int within_steps = -1) {
		
	}
	
	void allpairs(int from, int to, Var reach_var, int within_steps = -1) {
		//for now, reachesWithinSteps to be called instead
		while(from >= g_under.nodes() || to >= g_under.nodes()){
			newNode();
		}
		assert(from < g_under.nodes());
		if (within_steps > g_under.nodes())
			within_steps = -1;
		
		if (reach_info[from].source < 0) {
			
			addDetector((new AllPairsDetector<Weight>(detectors.size(), this, g_under, g_over, drand(rnd_seed))));
			//reach_detectors.push(reach_detectors.last());
			
			assert(detectors.last()->getID() == detectors.size() - 1);
			
			//reach_detectors.last()->negative_dist_detector = new Dijkstra(from,antig);
			//reach_detectors.last()->source=from;
			
			reach_info[from].source = from;
			reach_info[from].detector = detectors.last();
			
			//reach_detectors.last()->within=within_steps;
			
		}
		
		AllPairsDetector<Weight> * d = (AllPairsDetector<Weight>*) reach_info[from].detector;
		assert(d);
		
		d->addLit(from, to, reach_var, within_steps);
		
	}
	
	void reaches_private(int from, int to, Var reach_var, int within_steps = -1) {
		//for now, reachesWithinSteps to be called instead
		while(from >= g_under.nodes() || to >= g_under.nodes()){
			newNode();
		}
		if (within_steps >= 0 || opt_force_distance_solver) {
			reachesWithinSteps(from, to, reach_var, within_steps);
			return;
		}
		
		assert(from < g_under.nodes());
		if (within_steps > g_under.nodes())
			within_steps = -1;
		
		if (reach_info[from].source < 0) {
			
			ReachDetector<Weight>*rd = new ReachDetector<Weight>(detectors.size(), this, g_under, g_over, from,
					drand(rnd_seed));
			addDetector(rd);
			reach_detectors.push(rd);
			
			assert(detectors.last()->getID() == detectors.size() - 1);
			
			//reach_detectors.last()->negative_dist_detector = new Dijkstra(from,antig);
			//reach_detectors.last()->source=from;
			
			reach_info[from].source = from;
			reach_info[from].detector = detectors.last();
			
			//reach_detectors.last()->within=within_steps;
			
		}
		
		ReachDetector<Weight> * d = (ReachDetector<Weight>*) reach_info[from].detector;
		assert(d);
		assert(within_steps == -1);
		d->addLit(from, to, reach_var);
		
	}
	
	void reaches(int from, int to, Var reach_var, int within_steps = -1) {
		unimplemented_reachability_constraints.push( { from, to, within_steps, reach_var });
		//to allow us to alter the solving algorithm based on the number and type of constraints, we aren't implementing them here directly any more - instead,
		//we just store the constraints in this vector, then implement them later when 'implementConstraints' is called.
	}
	void distance(int from, int to, Var reach_var,  Weight distance_lt,bool inclusive) {
		unimplemented_distance_constraints.push( { from, to, distance_lt, reach_var,!inclusive });
		//to allow us to alter the solving algorithm based on the number and type of constraints, we aren't implementing them here directly any more - instead,
		//we just store the constraints in this vector, then implement them later when 'implementConstraints' is called.
	}

	void distanceBV(int from, int to, Var reach_var, int bvID, bool inclusive) {
		unimplemented_distance_constraints_bv.push( { from, to, bvID, reach_var, !inclusive});
		//to allow us to alter the solving algorithm based on the number and type of constraints, we aren't implementing them here directly any more - instead,
		//we just store the constraints in this vector, then implement them later when 'implementConstraints' is called.
	}
	void maxflowBV(int s, int t, Var reach_var, int bvID, bool inclusive) {
		unimplemented_maxflow_constraints_bv.push( { s, t, bvID, reach_var, !inclusive});
		//to allow us to alter the solving algorithm based on the number and type of constraints, we aren't implementing them here directly any more - instead,
		//we just store the constraints in this vector, then implement them later when 'implementConstraints' is called.
	}
	void reachesAny(int from, Var firstVar, int within_steps = -1) {
		for (int i = 0; i < g_under.nodes(); i++) {
			reaches(from, i, firstVar + i, within_steps);
		}
	}
	
	void reachesAny(int from, vec<Lit> & reachlits_out, int within_steps = -1) {
		for (int i = 0; i < g_under.nodes(); i++) {
			Var reachVar = S->newVar();
			//reaches(from,i,reachVar,within_steps);
			reaches(from, i, reachVar, within_steps);
			reachlits_out.push(mkLit(reachVar, false));
		}
	}


	//v will be true if the minimum weight is <= the specified value
	void minimumSpanningTree(Var v, Weight minimum_weight, bool inclusive) {
		if (!mstDetector) {
			mstDetector = new MSTDetector<Weight>(detectors.size(), this, g_under, g_over,  drand(rnd_seed));
			addDetector(mstDetector);

		}
		mstDetector->addWeightLit(v, minimum_weight,inclusive);
	}
	void edgeInMinimumSpanningTree(Var edgeVar, Var var) {
		if (!mstDetector) {
			mstDetector = new MSTDetector<Weight>(detectors.size(), this, g_under, g_over,  drand(rnd_seed));
			addDetector(mstDetector);

		}
		if (!S->hasTheory(edgeVar) || (S->getTheoryID(edgeVar) != getTheoryIndex())
				|| !isEdgeVar(S->getTheoryVar(edgeVar))) {
			fprintf(stderr, "%d is not an edge variable for theory %d! Aborting\n", edgeVar + 1, getTheoryIndex());
			exit(1);
		}
		edgeVar = S->getTheoryVar(edgeVar);
		int edgeid = getEdgeID(edgeVar);
		assert(edgeid >= 0);
		if (edge_list[edgeid].v == var_Undef) {
			printf("MST edge constraint for undefined edge %d with variable %d, aborting.\n", edgeid, edgeVar + 1);
			exit(1);
		}
		mstDetector->addTreeEdgeLit(edgeid, var);
	}
	void maxflow(int from, int to,Var v, Weight  max_flow,  bool inclusive=true) {
		while(from >= g_under.nodes() || to >= g_under.nodes()){
			newNode();
		}
		if(to >= g_under.nodes()){
			fprintf(stderr, "Undefined node %d\n",to);
			exit(1);
		}
		if(from >= g_under.nodes()){
			fprintf(stderr, "Undefined node %d\n",to);
			exit(1);
		}

		if(from==to){
			//The maxflow from a node to itself is always infinite, and so that flow is never less than any specific weight
			if(v!=var_Undef) {
				S->addClause(~mkLit(v));
			}
			return;
		}

		for (int i = 0; i < flow_detectors.size(); i++) {
			if (flow_detectors[i]->source == from && flow_detectors[i]->target == to) {
				flow_detectors[i]->addFlowLit(max_flow, v,inclusive);
				return;
			}
		}
		MaxflowDetector<Weight> *f = new MaxflowDetector<Weight>(detectors.size(), this,  g_under, g_over, from,
				to, drand(rnd_seed));
		flow_detectors.push(f);
		addDetector(f);

		f->addFlowLit(max_flow, v,inclusive);
	}

	void minConnectedComponents(int min_components, Var v) {
		if (!component_detector) {
			component_detector = new ConnectedComponentsDetector<Weight>(detectors.size(), this, g_under, g_over,
					drand(rnd_seed));
			addDetector(component_detector);

		}
		component_detector->addConnectedComponentsLit(v, min_components);
	}
	void addDetector(Detector* detector){
		detectors.push(detector);
		satisfied_detectors.push(false);
	}
	void acyclic(Var v,bool directed) {
		if (!cycle_detector) {
			cycle_detector = new CycleDetector<Weight>(detectors.size(), this, g_under, g_over, true, drand(rnd_seed));
			addDetector(cycle_detector);
		}
		cycle_detector->addAcyclicLit(directed, v);
	}
	
	void steinerTree(const vec<std::pair<int, Var> > & terminals, int steinerTreeID) {
		steiner_detectors.growTo(steinerTreeID + 1);
		assert(!steiner_detectors[steinerTreeID]);
		steiner_detectors[steinerTreeID] = new SteinerDetector<Weight>(detectors.size(), this,  g_under, g_over,
				drand(rnd_seed));
		addDetector(steiner_detectors[steinerTreeID]);
		for (int i = 0; i < terminals.size(); i++) {
			steiner_detectors[steinerTreeID]->addTerminalNode(terminals[i].first, terminals[i].second);
		}
	}
	
	void addSteinerWeightConstraint(int steinerTreeID, Weight weight, Var outerVar) {
		if (steinerTreeID >= steiner_detectors.size()) {
			fprintf(stderr, "invalid steinerTreeID %d\n", steinerTreeID);
			exit(1);
		}
		steiner_detectors[steinerTreeID]->addWeightLit(weight, outerVar);
	}
	
	/*	void inTerminalSet(int node, int terminalSet, Var outerVar){
	 terminalSets.growTo(terminalSet+1);
	 while(terminalSets[terminalSet].nodes()<node){
	 terminalSets[terminalSet].addNode();
	 }
	 Var v= newVar(outerVar,node,true);
	 Lit l = mkLit(v,false);
	 if(terminalSets[terminalSet].getNodeVar(node)<0){
	 terminalSets[terminalSet].setNodeVar(v);
	 }
	 }*/

	void printSolution() {
		
		for (auto * d : detectors) {
			assert(d);
			d->printSolution();
		}
	}
	void addTheory(Theory * t){
		theories.push(t);
	}

	bool isConstant(Var v)const{
		return S->isConstant(toSolver(v));
	}


	virtual CRef reason(Var v)const{
		return S->reason(toSolver(v));
	}


	bool addConflictClause(vec<Lit> & ps, CRef & confl_out, bool permanent) {
		toSolver(ps);
		return S->addConflictClause(ps,confl_out,permanent);
	}

	CRef newReasonMarker(Heuristic * theory,bool is_decision=false) override{
		CRef reasonMarker = S->newReasonMarker(is_decision ? theory:this, is_decision);
		int mnum = CRef_Undef - reasonMarker;
		marker_map.growTo(mnum + 1);
		marker_map[mnum].forTheory=!is_decision;
        marker_map[mnum].forHeuristic=is_decision;
		marker_map[mnum].id = is_decision ? theory->getHeuristicIndex() :  theory->getTheoryIndex();
		return reasonMarker;
	}

	bool hasTheory(Var v) {
		return vars[v].isTheoryVar;
	}
	bool hasTheory(Lit l) {
		return vars[var(l)].isTheoryVar;
	}
	int getTheoryID(Var v) {
		return vars[v].detector_edge - 1;
	}
	int getTheoryID(Lit l) {
		return vars[var(l)].detector_edge - 1;
	}
	Var getTheoryVar(Var v) {
		assert(hasTheory(v));
		return (Var) vars[v].theory_var;
	}

	//Translate a literal into its corresponding theory literal (if it has a theory literal)
	Lit getTheoryLit(Lit l) {
		assert(hasTheory(l));
		return mkLit(getTheoryVar(var(l)), sign(l));
	}

	void needsPropagation(int theoryID){
		requiresPropagation=true;
		S->needsPropagation(getTheoryIndex());
	}

	bool check_propagated()override{
		return !requiresPropagation;
	}
	Weight getModel_MaximumFlow(Lit theoryLit){
		Var v = var(theoryLit);
		Detector * d= detectors[getDetector(v)];
		MaxflowDetector<Weight> * mf = dynamic_cast<MaxflowDetector<Weight>*>(d);
		if(!mf)
			return -1;
		return mf->getModel_Maxflow();
	}
	Weight getModel_MaximumFlow_EdgeFlow(Lit theoryLit, Lit edgeLit){
		Var v = var(theoryLit);
		assert(isEdgeVar(var(edgeLit)));
		int edgeID =  getEdgeID(var(edgeLit));
		Detector * d= detectors[getDetector(v)];
		MaxflowDetector<Weight> * mf = dynamic_cast<MaxflowDetector<Weight>*>(d);
		if(!mf)
			return -1;
		return mf->getModel_EdgeFlow(edgeID);
	}
	Weight getModel_MaximumFlow_AcyclicEdgeFlow(Lit theoryLit, Lit edgeLit){
		Var v = var(theoryLit);
		assert(isEdgeVar(var(edgeLit)));
		int edgeID =  getEdgeID(var(edgeLit));
		Detector * d= detectors[getDetector(v)];
		MaxflowDetector<Weight> * mf = dynamic_cast<MaxflowDetector<Weight>*>(d);
		if(!mf)
			return -1;
		return mf->getModel_AcyclicEdgeFlow(edgeID);
	}
	Weight getModel_MinimumSpanningWeight(Lit theoryLit){
		Var v = var(theoryLit);
		Detector * d= detectors[getDetector(v)];
		MSTDetector<Weight> * mst = dynamic_cast<MSTDetector<Weight>*>(d);
		if(!mst)
			return -1;
		return mst->getModel_SpanningTreeWeight();
	}




	//Get a valid path, in terms of nodes, (from a reachability or shortest path constraint)
	//store_path must point to an array of ints of sufficient length to store the path (the path length can be optained by a call to getModel_PathLength)
	//Or, return false if there is no such path
	bool getModel_Path(Lit theoryLit, std::vector<int> & store_path){
		store_path.clear();
		Var v = var(theoryLit);
		Detector * d= detectors[getDetector(v)];
		//this is awful, fix this!!
		ReachDetector<Weight> * r = dynamic_cast<ReachDetector<Weight>*>(d);
		if(!r){
			DistanceDetector<Weight> * dist = dynamic_cast<DistanceDetector<Weight>*>(d);
			if(!dist){
				WeightedDistanceDetector<Weight> * dist = dynamic_cast<WeightedDistanceDetector<Weight>*>(d);
				if(!dist){
					assert(false);
					return false;
				}else{
					int node = dist->getNode(v);
					return dist->getModel_Path(node,store_path);
				}
			}else{
				int node = dist->getNode(v);
				return dist->getModel_Path(node,store_path);
			}
		}
		int node = r->getNode(v);
		return r->getModel_Path(node,store_path);
	 }
	 //Get a valid path, in terms of edges, (from a reachability or shortest path constraint)
	 //store_path must point to an array of ints of sufficient length to store the path (the path length can be optained by a call to getModel_PathLength)
	//Or, return false if there is no such path
	bool getModel_PathByEdgeLit(Lit theoryLit, std::vector<Lit> & store_path){
		store_path.clear();
		Var v = var(theoryLit);
		Detector * d= detectors[getDetector(v)];
		ReachDetector<Weight> * r = dynamic_cast<ReachDetector<Weight>*>(d);
		if(!r){
			DistanceDetector<Weight> * dist = dynamic_cast<DistanceDetector<Weight>*>(d);
			if(!dist){
				WeightedDistanceDetector<Weight> * dist = dynamic_cast<WeightedDistanceDetector<Weight>*>(d);
				if(!dist){
					assert(false);
					return false;
				}else{
					int node = dist->getNode(v);
					return dist->getModel_PathByEdgeLit(node,store_path);
				}
			}else{
				int node = dist->getNode(v);
				return dist->getModel_PathByEdgeLit(node,store_path);
			}
		}
		int node = r->getNode(v);
		return r->getModel_PathByEdgeLit(node,store_path);
	}
};

}
;

#endif /* GRAPH_THEORY_H_ */
