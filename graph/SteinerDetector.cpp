/*
 * SteinerDetector.c
 *
 *  Created on: 2014-01-08
 *      Author: sam
 */



#include "SteinerDetector.h"
#include "GraphTheory.h"
#include "dgl/SteinerApprox.h"
#include "dgl/graph/DynamicNodes.h"
#include <limits>
#include <set>
#include "core/Config.h"
using namespace Minisat;
SteinerDetector::SteinerDetector(int detectorID, GraphTheorySolver * outer,  DynamicGraph &g,DynamicGraph &antig ,double seed):
Detector(detectorID),outer(outer),g(g),antig(antig),rnd_seed(seed),edge_weights(edge_weights),positive_reach_detector(NULL),negative_reach_detector(NULL){
	checked_unique=false;
	all_unique=true;
	positiveStatus = new SteinerDetector::SteinerStatus(*this,true);
	negativeStatus = new SteinerDetector::SteinerStatus(*this,false);
	positive_reach_detector =  new SteinerApprox<DynamicNodes,SteinerDetector::SteinerStatus>(g,underTerminalSet,*positiveStatus,1);//new SpiraPan<SteinerDetector::MSTStatus>(_g,*(positiveReachStatus),1);
	negative_reach_detector = new SteinerApprox<DynamicNodes,SteinerDetector::SteinerStatus>(antig,overTerminalSet,*negativeStatus,-1);


	reach_marker=outer->newReasonMarker(getID());
	non_reach_marker=outer->newReasonMarker(getID());

	reach_edge_marker=outer->newReasonMarker(getID());
	non_reach_edge_marker=outer->newReasonMarker(getID());
	first_reach_var=var_Undef;
}

void SteinerDetector::addTerminalNode(int node, Var outer_Var){
	Var var = outer->newVar(outer_Var,getID());
	underTerminalSet.addNode(node);
	underTerminalSet.setNodeEnabled(node,false);
	overTerminalSet.addNode(node);
	overTerminalSet.setNodeEnabled(node,true);
	terminal_map.growTo(g.nodes());
	terminal_map[node]=var;
	//vec<int> terminal_var_map;
	terminal_var_map.growTo(var+1,var_Undef);
	terminal_var_map[var]=node;
}

void SteinerDetector::addWeightLit(Var outer_weight_var,int min_weight){
	g.invalidate();
	antig.invalidate();

	//while( dist_lits[to].size()<=within_steps)
	//	dist_lits[to].push({lit_Undef,-1});
	Var weight_var = outer->newVar(outer_weight_var,getID());

	Lit reachLit=mkLit(weight_var,false);
	bool found=false;
	for(int i = 0;i<weight_lits.size();i++){
		if(weight_lits[i].min_weight==min_weight){
			found=true;
			Lit r = weight_lits[i].l;
			//force equality between the new lit and the old reach lit, in the SAT solver
			outer->makeEqual(r,reachLit);
			/*outer->S->addClause(~r, reachLit);
			outer->S->addClause(r, ~reachLit);*/
		}
	}
	if(!found){
		weight_lits.push();
		weight_lits.last().l = reachLit;
		weight_lits.last().min_weight=min_weight;

		//weight_lit_map.insert(min_weight,weight_lits.size()-1);
	}



}

void SteinerDetector::SteinerStatus::setMinimumSteinerTree(int weight){

	for(int i = 0;i<detector.weight_lits.size();i++){
		int min_weight =  detector.weight_lits[i].min_weight;
		Lit l = detector.weight_lits[i].l;
		if(l!=lit_Undef){
			assert(l!=lit_Undef);
			if(min_weight<weight && !polarity){
				lbool assign = detector.outer->value(l);
				if( assign!= l_False ){
					detector.changed_weights.push({~l,min_weight});
				}
			}else if(min_weight>=weight && polarity){
				lbool assign = detector.outer->value(l);
				if( assign!= l_True ){
					detector.changed_weights.push({l,min_weight});
				}
			}
		}
	}

}
void SteinerDetector::buildMinWeightTooSmallReason(int weight,vec<Lit> & conflict){


		}


		void SteinerDetector::buildMinWeightTooLargeReason(int weight,vec<Lit> & conflict){



		}




		void SteinerDetector::buildReason(Lit p, vec<Lit> & reason, CRef marker){

				if(marker==reach_marker){
					reason.push(p);

					Var v = var(p);
					int weight=-1;
					//could swap this out for a map if there are lots of lits..
					for(int i = 0;i<weight_lits.size();i++){
						if(var(weight_lits[i].l)==v){
							weight=weight_lits[i].min_weight;
							break;
						}
					}
					assert(weight>=0);
					buildMinWeightTooSmallReason(weight,reason);

					//double elapsed = rtime(2)-startpathtime;
				//	pathtime+=elapsed;
				}else if(marker==non_reach_marker){
					reason.push(p);

					//the reason is a cut separating p from s;
					//We want to find a min-cut in the full graph separating, where activated edges (ie, those still in antig) are weighted infinity, and all others are weighted 1.

					//This is a cut that describes a minimal set of edges which are disabled in the current graph, at least one of which would need to be activated in order for s to reach p
					//assign the mincut edge weights if they aren't already assigned.


					Var v = var(p);

					int weight=-1;
					//could swap this out for a map if there are lots of lits..
					for(int i = 0;i<weight_lits.size();i++){
						if(var(weight_lits[i].l)==v){
							weight=weight_lits[i].min_weight;
							break;
						}
					}
					assert(weight>=0);

					buildMinWeightTooLargeReason(weight,reason);

				}else{
					assert(false);
				}
		}

		bool SteinerDetector::propagate(vec<Lit> & conflict){
			static int it = 0;
			if(++it==7){
				int a = 1;
			}

			double startdreachtime = rtime(2);
			stats_under_updates++;
			positive_reach_detector->update();
			double reachUpdateElapsed = rtime(2)-startdreachtime;
			stats_under_update_time+=reachUpdateElapsed;
		//}else
		//	stats_skipped_under_updates++;

		//if(negative_reach_detector && (!opt_detect_pure_theory_lits || unassigned_negatives>0)){
			double startunreachtime = rtime(2);
			stats_over_updates++;
			negative_reach_detector->update();
			double unreachUpdateElapsed = rtime(2)-startunreachtime;
			stats_over_update_time+=unreachUpdateElapsed;
		//}else
		//	stats_skipped_over_updates++;



		for(int j = 0;j<changed_weights.size();j++){
			Lit l = changed_weights[j].l;
			//printf("mst: %d\n",dimacs(l));
			int weight = changed_weights[j].weight;

			bool reach = !sign(l);
			if(outer->value(l)==l_True){
				//do nothing
			}else if(outer->value(l)==l_Undef){
				//trail.push(Assignment(false,reach,detectorID,0,var(l)));
				if(reach)
					outer->enqueue(l,reach_marker) ;
				else
					outer->enqueue(l,non_reach_marker) ;

			}else if (outer->value(l)==l_False){
				conflict.push(l);

				if(reach){

				//conflict
				//The reason is a path in g from to s in d
					buildMinWeightTooSmallReason(weight,conflict);
				//add it to s
				//return it as a conflict

				}else{

					buildMinWeightTooLargeReason(weight,conflict);

				}

				return false;
			}else{
				int  a=1;
			}

		}

			return true;
		}

bool SteinerDetector::checkSatisfied(){
	Kruskal<MinimumSpanningTree::NullStatus> positive_checker(g,MinimumSpanningTree::nullStatus,0);
	Kruskal<MinimumSpanningTree::NullStatus> negative_checker(antig,MinimumSpanningTree::nullStatus,0);
	positive_checker.update();
	negative_checker.update();
	for(int k = 0;k<weight_lits.size();k++){
		Lit l = weight_lits[k].l;
		int dist = weight_lits[k].min_weight;

		if(l!=lit_Undef){


			if(outer->value(l)==l_True){
				if(positive_checker.weight()>dist){
					return false;
				}
			}else if (outer->value(l)==l_False){
				if( negative_checker.weight()<=dist){
					return false;
				}
			}else{
				if(positive_checker.weight()<=dist){
					return false;
				}
				if(!negative_checker.weight()>dist){
					return false;
				}
			}
		}
	}

	return true;
}
Lit SteinerDetector::decide(){
	/*SteinerDetector *r =this;
	MinimumSpanningTree<SteinerDetector::MSTStatus> * over = (MinimumSpanningTree<SteinerDetector::MSTStatus>*) r->negative_reach_detector;

	MinimumSpanningTree<SteinerDetector::MSTStatus> * under = (MinimumSpanningTree<SteinerDetector::MSTStatus>*) r->positive_reach_detector;

	//we can probably also do something similar, but with cuts, for nodes that are decided to be unreachable.

	//ok, for each node that is assigned reachable, but that is not actually reachable in the under approx, decide an edge on a feasible path

	//this can be obviously more efficient
	//for(int j = 0;j<nNodes();j++){
	if(opt_decide_graph_neg){

	}

	for(int k = 0;k<dist_lits.size();k++){
		for(int n = 0;n<dist_lits[k].size();n++){
			Lit l = dist_lits[k][n].l;
			int min_weight = dist_lits[k][n].min_weight;
			if(l==lit_Undef)
				continue;
			int j = r->getNode(var(l));
			if(outer->value(l)==l_True){
				if(opt_decide_graph_pos){
				//if(S->level(var(l))>0)
				//	continue;

				assert(over->distance(j)<=min_dist);//else we would already be in conflict before this decision was attempted!
				if(under->distance(j)>min_dist){
					//then lets try to connect this
					//static vec<bool> print_path;

					assert(over->connected(j));//Else, we would already be in conflict


					int p =j;
					int last=j;
					if(!opt_use_random_path_for_decisions)
					{
						//ok, read back the path from the over to find a candidate edge we can decide
						//find the earliest unconnected node on this path
						over->update();
						 p = j;
						 last = j;
						 int dist = 0;
						while(under->distance(p)>=min_dist-dist){

							last=p;
							assert(p!=r->source);
							int prev = over->previous(p);
							assert(over->distance(p)<=min_dist-dist);
							dist+=1;//should really be weighted
							p = prev;

						}
					}else{
					//This won't work (without modification) because we need to constrain these paths to ones of maximum real distance < min_dist.
						//Randomly re-weight the graph sometimes
						if(drand(rnd_seed)<opt_decide_graph_re_rnd){

							for(int i=0;i<outer->g.nodes();i++){
									 double w = drand(rnd_seed);
									 w-=0.5;
									 w*=w;
									 //printf("%f (%f),",w,rnd_seed);
									 rnd_path->setWeight(i,w);
								 }
						}
						 rnd_path->update();
						//derive a random path in the graph
						 p = j;
						 last = j;
						 assert( rnd_path->connected(p));
						while(!under->connected(p)){

							last=p;
							assert(p!=source);
							int prev = rnd_path->previous(p);
							p = prev;
							assert(p>=0);
						}

					}



					//ok, now pick some edge p->last that will connect p to last;
					assert(!under->connected(last));
					assert(under->connected(p));

					assert(over->connected(last));
					assert(over->connected(p));
					assert(p>-1);
					if(p>-1){
					Var v = outer->edges[p][last].v;
					if(outer->value(v)==l_Undef){
						return mkLit(v,false);
					}else{
						assert(outer->value(v)!=l_True);
					}
					}
					for(int k = 0;k<outer->antig.adjacency[p].size();k++){
						int to = outer->antig.adjacency[p][k].node;
						if (to==last){
							Var v =outer->edge_list[ outer->antig.adjacency[p][k].id].v;
							if(outer->value(v)==l_Undef){
								return mkLit(v,false);
							}else{
								assert(outer->value(v)!=l_True);
							}
						}
					}

				}
				}
			}else if(outer->value(l)==l_False){
				if(opt_decide_graph_neg){

					//assert(over->distance(j)<=min_dist);//else we would already be in conflict before this decision was attempted!


					if(over->distance(j)<=min_dist && under->distance(j)>min_dist){
						//then lets try to disconnect this node from source by walking back along the path in the over approx, and disabling the first unassigned edge we see.
						//(there must be at least one such edge, else the variable would be connected in the under approximation as well - in which case it would already have been propagated.
						int p = j;
						int last = j;
						 int dist = 0;
						// tmp_nodes.clear();
						while(under->distance(p)>min_dist-dist){
							//int d = under->distance(p);
							//int d_over = over->distance(p);
							last=p;
							assert(p!=source);
							int prev = over->previous(p);
							Var v = outer->edges[prev][p].v;
							if(outer->value(v)==l_Undef){
								//if(opt_use_random_path_for_decisions)
								//	tmp_nodes.push(v);
								//else
								return mkLit(v,true);
							}else{
								assert(outer->value(v)!=l_False);
							}
							assert(over->distance(p)<=min_dist-dist);
							dist+=1;//should really be weighted
							p = prev;

						}
						assert(opt_use_random_path_for_decisions);
						assert(tmp_nodes.size()>0);
						int i = irand(rnd_seed,tmp_nodes.size());
						Var v= tmp_nodes[i];
						return mkLit(v,true);

					}
				}
			}
		}
	}*/
	return lit_Undef;
};


