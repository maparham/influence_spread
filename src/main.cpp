#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <map>
#include <time.h>
#include <functional>

#include <Snap.h>

#define __DEBUG__ 1
#include <utils.h>

using namespace std;
using namespace TSnap;

#define INF numeric_limits<int>::max()
#define WEIGHTATTR "weight"

void forInput(function<void(const string&)> fn) {
	clock_t begin = clock();

	std::ifstream filelist("input_list.txt");
	string path;
	while (filelist >> path) {
		if (path[0] == '#') {
			continue;
		}
		clock_t t = clock();

		fn(path);

		clock_t now = clock();
		clock_t elapsed_secs = double(now - t) / CLOCKS_PER_SEC;
		if (elapsed_secs < 60) {
			PRINTF("elapsed time=%d sec\n", elapsed_secs);
		} else {
			PRINTF("elapsed time=%d min\n", elapsed_secs / 60);
		}
	}

	clock_t end = clock();
	clock_t elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
	PRINTF("total time=%d min\n", elapsed_secs / 60);
}

class ICModel {
	const MyGraph& G;
	const int R;
	const TStr& dataPath;
	TIntStrH NIdColorH;
	TIntStrH EIdColorH;

	void resetColors() {
		NIdColorH.Clr(true);
		EIdColorH.Clr(true);
		for (EdgeI EI = G->BegEI(); EI != G->EndEI(); EI++) {
			EIdColorH.AddDat(EI.GetId(), "gray"); // default edge color
		}
	}

	// stochastic spread simulation
	size_t diffusion_spread(const vector<NodeI>& seedSet,
			const bool deterministic = false, const bool draw = false) {

		assert(seedSet.size() > 0);
		list<NodeI> q(seedSet.begin(), seedSet.end());
		resetColors();
		map<int, bool> active;
		size_t spread = 0;
		for (NodeI NI : seedSet) {
			active[NI.GetId()] = true;
			NIdColorH.AddDat(NI.GetId(), "yellow");
		}

		int frame = 0;
		if (draw) {
			draw_graph(frame++);
		}
		while (!q.empty()) {

			NodeI NI = q.back();
			q.pop_back();
			int out_degree = NI.GetOutDeg();

			TStr color;
			if (draw) {
				// highlight the influencing node
//				color = NIdColorH.GetDat(NI.GetId());
//				NIdColorH.AddDat(NI.GetId(), "green");
			}

//			PRINTF("Node %d, out degree=%d\n", NI.GetId(), out_degree);
			while (out_degree-- > 0) {
				NodeI out_neighbor = G->GetNI(NI.GetOutNId(out_degree));
				if (active.find(out_neighbor.GetId()) != active.end()) { // already active
					continue;
				}

				const int in_degree = out_neighbor.GetInDeg();
//				PRINTF("out neighbor %d has in degree=%d, spread=%d\n",
//						out_neighbor.GetId(), in_degree, spread);
				if (deterministic || rand() % in_degree == 0) { // true with probability 1/in_degree
					q.push_front(out_neighbor);
					active[out_neighbor.GetId()] = true;
					if (draw) {
						NIdColorH.AddDat(NI.GetOutNId(out_degree), "blue");
						EIdColorH.AddDat(NI.GetOutEId(out_degree), "blue");
						draw_graph(frame++);
						NIdColorH.AddDat(out_neighbor.GetId(), "black");
						EIdColorH.AddDat(NI.GetOutEId(out_degree), "black");
					}
					++spread;
					assert(spread <= G->GetNodes());
				}
			}
//			NIdColorH.AddDat(NI.GetId(), color); // restore the original color
		}
		return spread;
	}

	// Monte Carlo estimate of expected spread
	float MC_estimate(const vector<NodeI>& seedSet) {
		size_t sum = 0;
		int r = R;
		while (r-- > 0) {
			size_t s = diffusion_spread(seedSet);
			//PRINTF("r=%d, s=%d\n", r, s);
			sum += s;
		}
		return (float) sum / R;
	}
	bool hasNode(const vector<NodeI>& v, const NodeI& NI) {
		for (NodeI _NI : v) {
			if (_NI.GetId() == NI.GetId()) {
				return true;
			}
		}
		return false;
	}

public:
	const char* title_template =
			"spread ratio=%d%%, %d nodes, %d edges\n Yellow: seed, Black: activated, Blue: activating\n";
	char title[200];

	void setTitle(const int ratio = 0) {
		sprintf(title, title_template, ratio, G->GetNodes(), G->GetEdges());
	}

	ICModel(const MyGraph& G, const int R, const TStr& pathobj) :
			G(G), R(R), dataPath(pathobj) {
		assert(R > 0);
		setTitle();
	}

	void draw_graph(const int frameNo) {
		TStr imagePath = dataPath + "_/" + "image.png";
		DrawGViz(G, TGVizLayout::gvlDot,
				imagePath.GetNumFNm(imagePath, frameNo), title, false,
				NIdColorH, EIdColorH);
	}

	tuple<vector<NodeI>, float> seed_selection(int k) {
		vector<NodeI> chosen;
		float spread;
		while (k-- > 0) {
			NodeI best = G->BegNI();
			spread = 0;
			for (NodeI NI = G->BegNI(); NI != G->EndNI(); NI++) {
				if (hasNode(chosen, NI)) {
					continue;
				}
				vector<NodeI> testSeeds(chosen.begin(), chosen.end());
				testSeeds.push_back(NI);
				float tmp = MC_estimate(testSeeds);
				if (spread < tmp) {
					spread = tmp;
					best = NI;
				}
			}

			chosen.push_back(best);
		}
		return make_tuple(chosen, spread);
	}
	// the maximum possible spread using the seedSet
	size_t deterministic_spread(const vector<NodeI>& seedSet) {
		return diffusion_spread(seedSet, true);
	}
	size_t simulate(const vector<NodeI>& seedSet) {
		return diffusion_spread(seedSet, false, true);
	}
};

void advance(EdgeI& EI, int n) {
	while (n-- > 0) {
		EI++;
	}
}

// scale down the edge set
void sparsify(MyGraph& G, const int percent) {
	int b = G->GetEdges() - ((float) percent / 100) * G->GetEdges();
	// sparsify by deleting edges randomly
	//TRnd(time(NULL)).Randomize(); // this doesn't work!
	while (b-- > 0) {
		auto rndEI = G->BegEI();
		advance(rndEI, rand() % G->GetEdges());
		G->DelEdge(rndEI.GetId());
	}
}

int main() {
	init();
//	ofstream resultFile(Reporter::resultDir() + "overall.txt");

	const int R = 100; // Monte Carlo iterations
	const int SEEDS = 3;

	forInput(
			[](const string& path) {
				printf("\nLoading %s\n", path.c_str());
				const TStr pathobj(path.c_str());

				MyGraph G = LoadEdgeList<MyGraph>(pathobj, 0, 1);
				sparsify(G,20);

				char title[20];
				sprintf(title,"%d nodes, %d edges\n",G->GetNodes(),G->GetEdges());
				PRINTF("%s\n",title);

				ICModel model(G, R, pathobj);
				auto [seeds, spread] = model.seed_selection(SEEDS);

				const size_t maxSpread = model.deterministic_spread(seeds);
				int ratio =100*spread/maxSpread;
				PRINTF("spread=%f/%d, ratio=%d%%, seeds=%d\n", spread, maxSpread, ratio,seeds.size() );
				model.setTitle(ratio);

				PRINTF("drawing and simulation...\n");
//			model.draw_graph(0);

				spread = model.simulate(seeds);
				ratio =100*spread/maxSpread;
				PRINTF("spread1=%f, ratio1=%d%%\n", spread, ratio);
			});
	return 0;
}
