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

#ifndef EDMONDS_KARP_ADJ_H
#define EDMONDS_KARP_ADJ_H


#include "MaxFlow.h"
#include <vector>
#include <algorithm>
#include "core/Config.h"
#include "dgl/EdmondsKarp.h"

namespace dgl{
template< class Capacity, typename Weight=int  >
class EdmondsKarpAdj:public MaxFlow<Weight>{

public:

	std::vector<Weight> F;
	struct LocalEdge{
		int from;
		int id;
		bool backward=false;
		LocalEdge(int from=-1, int id=-1, bool backward=false):from(from),id(id),backward(backward){

		}
	};
	Weight curflow;
    int last_modification;
    int last_deletion;
    int last_addition;

    int history_qhead;
    int last_history_clear;
    std::vector<LocalEdge> prev;
    std::vector<Weight> M;
    DynamicGraph& g;
    Capacity & capacity;
    Weight INF;
#ifdef DEBUG_MAXFLOW
    	EdmondsKarp<Weight> ek;
#endif

    std::vector<int> Q;




    Weight  BreadthFirstSearch(int s, int t){
     	for(int i = 0;i<g.nodes();i++)
     		prev[i].from=-1;
     	prev[s].from = -2;
    	Q.clear();
           Q.push_back(s);

       	for(int j = 0;j<Q.size();j++){
   			   int u = Q[j];

               for (int i = 0;i<g.nIncident(u);i++){
            	   if(!g.edgeEnabled(g.incident(u,i).id))
						continue;
            	   int id = g.incident(u,i).id;
            	   int v = g.incident(u,i).node;
                   ///(If there is available capacity, and v is not seen before in search)

            	   Weight &f = F[id];
            	   Weight  c = capacity[id];

            	 //  int fr = F[id];
                   if (((c - F[id]) > 0) && (prev[v].from == -1)){
                       prev[v] = LocalEdge(u,id,false);
                       Weight b=c - F[id];
                       M[v] = std::min(M[u],b );
                       if (v != t)
                           Q.push_back(v);
                       else
                           return M[t];
                   }
               }

               for (int i = 0;i<g.nIncoming(u);i++){
            	   int id = g.incoming(u,i).id;
            	   if(!g.edgeEnabled(id))
						continue;

				   int v = g.incoming(u,i).node;

				   Weight f = 0;
				   Weight& c = F[id];

					  if (((c - f) > 0) && (prev[v].from == -1)){
						  prev[v] = LocalEdge(u,id,true);
						  Weight b = c-f;
						  M[v] = std::min(M[u], b);
						  if (v != t)
							  Q.push_back(v);
						  else
							  return M[t];
					  }
				  }
           }
           return 0;


	   }
public:
    EdmondsKarpAdj(DynamicGraph& _g,Capacity & cap):g(_g),capacity(cap),INF(0xF0F0F0)
#ifdef DEBUG_MAXFLOW
    	,ek(_g)
#endif
    {
    	  curflow=0;
      	last_modification=-1;
      	last_deletion=-1;
      	last_addition=-1;

      	history_qhead=-1;
      	last_history_clear=-1;
    	//setAllEdgeCapacities(1);

    }
    void setCapacity(int u, int w, Weight c){
    	//C.resize(g.edges());
    	//C[ ]=c;

    }
    void setAllEdgeCapacities(Weight c){

    }
    void dbg_print_graph(int from, int to){
   #ifndef NDEBUG
    	return;
       		static int it = 0;
       		if(++it==6){
       			int a =1;
       		}
       		printf("Graph %d\n", it);
       			printf("digraph{\n");
       			for(int i = 0;i<g.nodes();i++){
       				if(i==from){
       					printf("n%d [label=\"From\", style=filled, fillcolor=blue]\n", i);
       				}else if (i==to){
       					printf("n%d [label=\"To\", style=filled, fillcolor=red]\n", i);
       				}else
       					printf("n%d\n", i);
       			}

       			for(int i = 0;i<g.edges();i++){
       				if(g.edgeEnabled(i)){
   						auto & e = g.all_edges[i];
   						const char * s = "black";
   						std::cout<<"n" << e.from <<" -> n" << e.to << " [label=\"" << i <<": " <<  F[i]<<"/" << capacity[i]  << "\" color=\"" << s<<"\"]\n";
   						//printf("n%d -> n%d [label=\"%d: %d/%d\",color=\"%s\"]\n", e.from,e.to, i, F[i],capacity[i] , s);
       				}
       			}

       			printf("}\n");
   #endif
       		}
	long num_updates=0;
	int numUpdates()const{
		return num_updates;
	}
    const Weight maxFlow(int s, int t){
    	Weight f = 0;
#ifdef RECORD
		if(g.outfile){
			fprintf(g.outfile,"f %d %d\n", s,t);
			fflush(g.outfile);
		}
#endif

    	//C.resize(g.nodes());
/*
#ifdef DEBUG_MAXFLOW
    	for(int i = 0;i<g.all_edges.size();i++){
    		int id = g.all_edges[i].id;
    		int cap = capacity[id];
    		int from =  g.all_edges[i].from;
    		int to =  g.all_edges[i].to;

    		ek.setCapacity(from,to,cap);
    	}
#endif
*/
      	if(last_modification>0 && g.modifications==last_modification){
/*
#ifdef DEBUG_MAXFLOW
    	int expected_flow =ek.maxFlow(s,t);
#endif

#ifdef DEBUG_MAXFLOW
    	assert(curflow==expected_flow);
#endif
*/
        			return curflow;
        		}
    	/*if(rev.size()<g.all_edges.size()){
    		rev.clear();

    		rev.resize(g.all_edges.size());
    		for(int i = 0;i<g.all_edges.size();i++){
    			rev[i]=-1;
    			int from = g.all_edges[i].from;
    			int to = g.all_edges[i].to;
    			for(int j = 0;j<g.adjacency[to].size();j++){
    				if(g.adjacency[to][j].node == from){
    					rev[i]=g.adjacency[to][j].id;
    					break;
    				}
    			}
    		}
    	}*/

    	F.clear();
    	F.resize(g.all_edges.size());
    	prev.resize(g.nodes());
    	M.resize(g.nodes());

    	for(int i = 0;i<g.nodes();i++){
    		prev[i].from =-1;
    		M[i]=0;
    	}
    	prev[s].from = -2;
    	 M[s] = INF;
        while(true){
        	Weight m= BreadthFirstSearch(s,t);

            if (m == 0)
                break;

            f = f + m;

            int v = t;
            while (v!=  s){
                int u = prev[v].from;
                int id = prev[v].id;
    			if(prev[v].backward){
					F[id] = F[id] - m;
				}else
					F[id] = F[id] + m;
                v = u;
            }

        }
/*
#ifdef DEBUG_MAXFLOW
    	int expected_flow =ek.maxFlow(s,t);
#endif

#ifdef DEBUG_MAXFLOW
    	assert(f==expected_flow);
#endif
*/
        //dbg_print_graph(s,t);
    	curflow=f;
    	num_updates++;
		last_modification=g.modifications;
		last_deletion = g.deletions;
		last_addition=g.additions;
		dbg_print_graph(s,t);
		history_qhead=g.history.size();
		last_history_clear=g.historyclears;
        return f;
    }


    std::vector<bool> seen;
    std::vector<bool> visited;

    const Weight minCut(int s, int t, std::vector<MaxFlowEdge> & cut){
    	const Weight f = maxFlow(s,t);
    	//ok, now find the cut
    	Q.clear();
		Q.push_back(s);
		seen.clear();
		seen.resize(g.nodes());
		seen[s]=true;
		dbg_print_graph(s,t);
		//explore the residual graph
		for(int j = 0;j<Q.size();j++){
		   int u = Q[j];

			for(int i = 0;i<g.nIncident(u);i++){
				if(!g.edgeEnabled(g.incident(u,i).id))
					continue;
				int v = g.incident(u,i).node;
				int id = g.incident(u,i).id;
				if(capacity[id] - F[id] == 0){
					cut.push_back(MaxFlowEdge{u,v,id});//potential element of the cut
				}else if(!seen[v]){
					Q.push_back(v);
					seen[v]=true;
				}
			}
			for(int i = 0;i<g.nIncoming(u);i++){
				if(!g.edgeEnabled(g.incoming(u,i).id))
					continue;
				int v = g.incoming(u,i).node;
				int id = g.incoming(u,i).id;
				if(F[id] == 0){

				}else if(!seen[v]){
					Q.push_back(v);
					seen[v]=true;
				}
			}
		}
		//Now keep only the edges from a seen vertex to an unseen vertex
		int i, j = 0;
		for( i = 0;i<cut.size();i++){
			if(!seen[cut[i].v] && seen[cut[i].u]){
				cut[j++]=cut[i];
			}
		}
		cut.resize(j);
#ifndef NDEBUG
		Weight dbg_sum = 0;
		for(int i = 0;i<cut.size();i++){
			int id = cut[i].id;
			assert(F[id]==capacity[id]);
			dbg_sum+=F[id];
		}
		assert(dbg_sum==f);
#endif
    	return f;
    }
    const  Weight getEdgeCapacity(int id){
     	assert(g.edgeEnabled(id));
     	return capacity[id];
     }
    const Weight getEdgeFlow(int id){
    	assert(g.edgeEnabled(id));
    	return F[id];// reserve(id);
    }
    const Weight getEdgeResidualCapacity(int id){
    	assert(g.edgeEnabled(id));
    	return  capacity[id]-F[id];// reserve(id);
    }
};
};
#endif

