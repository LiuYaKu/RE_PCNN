#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <cstdlib>
#include <map>
#include <cmath>
#include <pthread.h>
#include <iostream>

#include<assert.h>
#include<ctime>
#include<sys/time.h>

#include "init.h"
#include "test.h"

using namespace std;

double score = 0;
float alpha1;

struct timeval t_start,t_end; 
long start,ending;

void time_begin()
{
  
  gettimeofday(&t_start, NULL); 
  start = ((long)t_start.tv_sec)*1000+(long)t_start.tv_usec/1000; 
}
void time_end()
{
  gettimeofday(&t_end, NULL); 
  ending = ((long)t_end.tv_sec)*1000+(long)t_end.tv_usec/1000; 
  cout<<"time(s):\t"<<((double)ending-(double)start)/1000<<endl;
}


//句子编码
vector<float> train(int *sentence, int *trainPositionE1, int *trainPositionE2, int len, vector<int> &tip) {
	vector<float> r;
	for (int i = 0; i < dimensionC; i++) {
		int last = i * dimension * window;
		int lastt = i * dimensionWPE * window;
		float mx[3];
		int ti[3];
		for (int i1 = 0; i1<3; i1++)
			mx[i1] = -FLT_MAX;
		int i2 = 0;
		for (int i1 = -window+1; i1 < len; i1++) 
		{
			float res = 0;
			int tot = 0;
			int tot1 = 0;
			for (int j = i1; j < i1 + window; j++) //卷积操作 
			if (j>=0&&j<len)
			{
				int last1 = sentence[j] * dimension;
			 	for (int k = 0; k < dimension; k++) {
			 		res += matrixW1Dao[last + tot] * wordVecDao[last1+k];
			 		tot++;
			 	}
			 	int last2 = trainPositionE1[j] * dimensionWPE;
			 	int last3 = trainPositionE2[j] * dimensionWPE;
			 	for (int k = 0; k < dimensionWPE; k++) {
			 		res += matrixW1PositionE1Dao[lastt + tot1] * positionVecDaoE1[last2+k];
			 		res += matrixW1PositionE2Dao[lastt + tot1] * positionVecDaoE2[last3+k];
			 		tot1++;
			 	}
			}
			else
			{
				tot+=dimension;
				tot1+=dimensionWPE;
			}
		//	for (int i2=0; i2<3; i2++)
			if (res > mx[i2]) {//池化
				mx[i2] = res;
				ti[i2] = i1;
			}
			if (i1>=0&&trainPositionE1[i1]==-PositionMinE1)
				i2++;
			if (i1>=0&&trainPositionE2[i1]==-PositionMinE2)
				i2++;
			assert(i2<3);
		}
		assert(i2==2);
		for (int i1 = 0; i1<3; i1++)//加偏置向量
		{
			r.push_back(mx[i1]+matrixB1Dao[3*i+i1]);
			tip.push_back(ti[i1]);
		}
	}
	for (int i = 0; i < 3 * dimensionC; i++) {//非线性变换
		r[i] = CalcTanh(r[i]);
	}
	return r;
}

//随机梯度下降法优化模型更新参数
void train_gradient(int *sentence, int *trainPositionE1, int *trainPositionE2, int len, int e1, int e2, int r1, float alpha, vector<float> &r,vector<int> &tip, vector<float> &grad)
{
	for (int i = 0; i < 3 * dimensionC; i++) {
		if (fabs(grad[i])<1e-8)
			continue;
		int last = (i/3) * dimension * window;
		int tot = 0;
		int lastt = (i/3) * dimensionWPE * window;
		int tot1 = 0;
		float g1 = grad[i] * (1 -  r[i] * r[i]);
		for (int j = 0; j < window; j++)  
		if (tip[i]+j>=0&&tip[i]+j<len)
		{
			int last1 = sentence[tip[i] + j] * dimension;
			for (int k = 0; k < dimension; k++) {
				matrixW1[last + tot] -= g1 * wordVecDao[last1+k];
				wordVec[last1 + k] -= g1 * matrixW1Dao[last + tot];
				tot++;
			}
			int last2 = trainPositionE1[tip[i] + j] * dimensionWPE;
			int last3 = trainPositionE2[tip[i] + j] * dimensionWPE;
			for (int k = 0; k < dimensionWPE; k++) {
				matrixW1PositionE1[lastt + tot1] -= g1 * positionVecDaoE1[last2 + k];
				matrixW1PositionE2[lastt + tot1] -= g1 * positionVecDaoE2[last3 + k];
				positionVecE1[last2 + k] -= g1 * matrixW1PositionE1Dao[lastt + tot1];
				positionVecE2[last3 + k] -= g1 * matrixW1PositionE2Dao[lastt + tot1];
				tot1++;
			}
		}
		matrixB1[i] -= g1;
	}
}

//以包为单位处理句子
float train_bags(string bags_name)
{
	int bags_size = bags_train[bags_name].size();
	vector<vector<float> > rList;
	vector<vector<int> > tipList;
	tipList.resize(bags_size);
	int r1 = -1;
	for (int k=0; k<bags_size; k++)
	{
		tipList[k].clear();
		int i = bags_train[bags_name][k];
		if (r1==-1)
			r1 = relationList[i];
		else
			assert(r1==relationList[i]);
		rList.push_back(train(trainLists[i], trainPositionE1[i], trainPositionE2[i], trainLength[i], tipList[k]));//为包中每条句子编码
	}
	
	vector<float> f_r;	
	
	vector<int> dropout;
	for (int i = 0; i < 3 * dimensionC; i++) 
		dropout.push_back(rand()%2);
	vector<float> weight;
	float weight_sum = 0;
	for (int k=0; k<bags_size; k++)
	{
		float s = 0;
		for (int i = 0; i < 3 * dimensionC; i++) 
			s += rList[k][i] * matrixRelationDao[r1 * 3 * dimensionC + i] * wt;
		s = exp(s); 
		weight.push_back(s);
		weight_sum += s;
	}
	for (int k=0; k<bags_size; k++)
		weight[k]/=weight_sum;//计算权重


    //按权重排序。
	for (int k = 0; k<bags_size; k++)
	{

		float temp;
		int max_index = k;
		float max = weight[k];
		for (int j = k + 1; j<bags_size; j++)
		{
			if (weight[j]>max)
			{
				max = weight[j];
				max_index = j;
			}
		}
		if (max_index != k)
		{
			vector<float> t;
			for (int i = 0; i < 3*dimensionC; i++)
				t.push_back(rList[k][i]);
			for (int i = 0; i < 3*dimensionC; i++)
				rList[k][i] = rList[max_index][i];
			for (int i = 0; i < 3*dimensionC; i++)
				rList[max_index][i] = t[i];
			temp = weight[k];
			weight[k] = max;
			weight[max_index] = temp;
		}
	}
	
	vector<float> zuhe_weight;
	for (int i = 0; i < weight.size(); i++) {
		float temp = 0;
		for (int j = 0; j <= i; j++) {
			temp += weight[j];
		}
		zuhe_weight.push_back(temp);
	}

	//组合句子
	vector<vector<float> > zuhe_ju;
	for (int j = 0; j < bags_size; j++) {
		vector<float> r;
		r.resize(3*dimensionC);
		for (int i = 0; i < 3*dimensionC; i++)
			for (int k = 0; k <= j; k++)
				r[i] += rList[k][i] * (weight[k]/zuhe_weight[j]);
		zuhe_ju.push_back(r);
	}

	
	vector<float> zuhe_best;//选出的最好的组合
	int max_zuhe_index=0;
	double max_s=-1;
	double rt = 0;
	for (int k = 0; k<bags_size; k++)
	{
		f_r.clear();
		float sum = 0;
		for (int j = 0; j < relationTotal; j++) {
			float ss = 0;
			for (int i = 0; i < 3 * dimensionC; i++) {
				ss += dropout[i] * zuhe_ju[k][i] * matrixRelationDao[j * 3 * dimensionC + i];
			}
			ss += matrixRelationPrDao[j];
			f_r.push_back(exp(ss));
			sum += f_r[j];
		}
		double rt = (log(f_r[r1]) - log(sum));//目标函数
		if (rt > max_s) {
			max_zuhe_index = k;
			max_s = rt;
		}

	}
	f_r.clear();
	for (int i = 0; i < 3*dimensionC; i++)
		zuhe_best.push_back(zuhe_ju[max_zuhe_index][i]);
	for (int k = 0; k<=max_zuhe_index; k++)
		weight[k] /= zuhe_weight[max_zuhe_index];
	for (int k = max_zuhe_index+1; k < bags_size; k++)
		weight[k] = 0;

	float sum = 0;
	for (int j = 0; j < relationTotal; j++) {
		float ss = 0;
		for (int i = 0; i < 3 * dimensionC; i++) {
			ss += dropout[i] * zuhe_best[i] * matrixRelationDao[j * 3 * dimensionC + i];
		}
		ss += matrixRelationPrDao[j];
		f_r.push_back(exp(ss));
		sum += f_r[j];
	}
	rt = max_s;


    //计算梯度，更新参数
	vector<vector<float> > grad;
	grad.resize(bags_size);
	for (int k=0; k<bags_size; k++)
		grad[k].resize(3 * dimensionC);
	vector<float> g1_tmp;
	g1_tmp.resize(3 * dimensionC);
	for (int r2 = 0; r2<relationTotal; r2++)
	{	
		vector<float> r;
		r.resize(3 * dimensionC);
		for (int i = 0; i < 3 * dimensionC; i++) 
			for (int k=0; k<bags_size; k++)
				r[i] += rList[k][i] * weight[k];
		
		float g = f_r[r2]/sum*alpha1;
		if (r2 == r1)
			g -= alpha1;
		for (int i = 0; i < 3 * dimensionC; i++) 
		{
			float g1 = 0;
			if (dropout[i]!=0)
			{
				g1 += g * matrixRelationDao[r2 * dimensionC * 3 + i];
				matrixRelation[r2 * 3 * dimensionC + i] -= g * r[i];
			}
			g1_tmp[i]+=g1;
		}
		matrixRelationPr[r2] -= g;
	}
	for (int i = 0; i < 3 * dimensionC; i++) 
	{
		float g1 = g1_tmp[i];
		double tmp_sum = 0; 
		for (int k=0; k<bags_size; k++)
		{
			grad[k][i]+=g1*weight[k];
			grad[k][i]+=g1*rList[k][i]*weight[k]*matrixRelationDao[r1 * 3 * dimensionC + i] * wt;
			matrixRelation[r1 * 3 * dimensionC + i] += g1*rList[k][i]*weight[k]*rList[k][i] * wt;
			tmp_sum += rList[k][i]*weight[k];
		}	
		for (int k1=0; k1<bags_size; k1++)
		{
			grad[k1][i]-=g1*tmp_sum*weight[k1]*matrixRelationDao[r1 * 3 * dimensionC + i] * wt;
			matrixRelation[r1 * 3 * dimensionC + i] -= g1*tmp_sum*weight[k1]*rList[k1][i] * wt;
		}
	}

	for (int k=0; k<bags_size; k++)
	{
		int i = bags_train[bags_name][k];
		train_gradient(trainLists[i], trainPositionE1[i], trainPositionE2[i], trainLength[i], headList[i], tailList[i], relationList[i], alpha1,rList[k], tipList[k], grad[k]);
		
	}

	return rt;
}

int turn;

int test_tmp = 0;

vector<string> b_train;
vector<int> c_train;
double score_tmp = 0, score_max = 0;
pthread_mutex_t mutex1;

int tot_batch;
void* trainMode(void *id ) {
		unsigned long long next_random = (long long)id;
		test_tmp = 0;
	//	for (int k1 = batch; k1 > 0; k1--)
		while (true)
		{

			pthread_mutex_lock (&mutex1);
			if (score_tmp>=score_max)
			{
				pthread_mutex_unlock (&mutex1);
				break;
			}
			score_tmp+=1;
		//	cout<<score_tmp<<' '<<score_max<<endl;
			pthread_mutex_unlock (&mutex1);
			int j = getRand(0, c_train.size());
			//cout<<j<<'|';
			j = c_train[j];
			//cout<<j<<'|';
			//test_tmp+=bags_train[b_train[j]].size();
			//cout<<test_tmp<<' ';
			score += train_bags(b_train[j]);
		}
		//cout<<endl;
}

void train() {
	int tmp = 0;
	b_train.clear();
	c_train.clear();
	for (map<string,vector<int> >:: iterator it = bags_train.begin(); it!=bags_train.end(); it++)
	{
		int max_size = 1;//it->second.size()/2;
		for (int i=0; i<max(1,max_size); i++)
			c_train.push_back(b_train.size());
		b_train.push_back(it->first);
		tmp+=it->second.size();
	}
	cout<<c_train.size()<<endl;

	float con = sqrt(6.0/(dimensionC+relationTotal));
	float con1 = sqrt(6.0/((dimensionWPE+dimension)*window));
	matrixRelation = (float *)calloc(3 * dimensionC * relationTotal, sizeof(float));
	matrixRelationPr = (float *)calloc(relationTotal, sizeof(float));
	matrixRelationPrDao = (float *)calloc(relationTotal, sizeof(float));
	wordVecDao = (float *)calloc(dimension * wordTotal, sizeof(float));
	positionVecE1 = (float *)calloc(PositionTotalE1 * dimensionWPE, sizeof(float));
	positionVecE2 = (float *)calloc(PositionTotalE2 * dimensionWPE, sizeof(float));
	
	matrixW1 = (float*)calloc(dimensionC * dimension * window, sizeof(float));
	matrixW1PositionE1 = (float *)calloc(dimensionC * dimensionWPE * window, sizeof(float));
	matrixW1PositionE2 = (float *)calloc(dimensionC * dimensionWPE * window, sizeof(float));
	matrixB1 = (float*)calloc(3 * dimensionC, sizeof(float));

     // 随机初始化以下矩阵
	for (int i = 0; i < dimensionC; i++) {
		int last = i * window * dimension;
		for (int j = dimension * window - 1; j >=0; j--)
			matrixW1[last + j] = getRandU(-con1, con1);
		last = i * window * dimensionWPE;
		float tmp1 = 0;
		float tmp2 = 0;
		for (int j = dimensionWPE * window - 1; j >=0; j--) {
			matrixW1PositionE1[last + j] = getRandU(-con1, con1);
			tmp1 += matrixW1PositionE1[last + j]  * matrixW1PositionE1[last + j] ;
			matrixW1PositionE2[last + j] = getRandU(-con1, con1);
			tmp2 += matrixW1PositionE2[last + j]  * matrixW1PositionE2[last + j] ;
		}
		for (int j=0; j<3; j++)
		matrixB1[i] = getRandU(-con1, con1);
	}

	for (int i = 0; i < relationTotal; i++) 
	{
		matrixRelationPr[i] = getRandU(-con, con);				//add
		for (int j = 0; j < 3 * dimensionC; j++)
			matrixRelation[i * 3 * dimensionC + j] = getRandU(-con, con);
	}

	for (int i = 0; i < PositionTotalE1; i++) {
		float tmp = 0;
		for (int j = 0; j < dimensionWPE; j++) {
			positionVecE1[i * dimensionWPE + j] = getRandU(-con1, con1);
			tmp += positionVecE1[i * dimensionWPE + j] * positionVecE1[i * dimensionWPE + j];
		}
	}

	for (int i = 0; i < PositionTotalE2; i++) {
		float tmp = 0;
		for (int j = 0; j < dimensionWPE; j++) {
			positionVecE2[i * dimensionWPE + j] = getRandU(-con1, con1);
			tmp += positionVecE2[i * dimensionWPE + j] * positionVecE2[i * dimensionWPE + j];
		}
	}

	matrixRelationDao = (float *)calloc(3 * dimensionC*relationTotal, sizeof(float));
	matrixW1Dao =  (float*)calloc(dimensionC * dimension * window, sizeof(float));
	matrixB1Dao =  (float*)calloc(3 * dimensionC, sizeof(float));
	
	positionVecDaoE1 = (float *)calloc(PositionTotalE1 * dimensionWPE, sizeof(float));
	positionVecDaoE2 = (float *)calloc(PositionTotalE2 * dimensionWPE, sizeof(float));
	matrixW1PositionE1Dao = (float *)calloc(dimensionC * dimensionWPE * window, sizeof(float));
	matrixW1PositionE2Dao = (float *)calloc(dimensionC * dimensionWPE * window, sizeof(float));
	/*time_begin();
	test();
	time_end();*/
//	return;
	for (turn = 0; turn < trainTimes; turn ++) {

	//	len = trainLists.size();
		len = c_train.size();
		npoch  =  len / (batch * num_threads);
		alpha1 = alpha*rate/batch;

		score = 0;
		score_max = 0;
		score_tmp = 0;
		double score1 = score;
		time_begin();
		for (int k = 1; k <= npoch; k++) {
			score_max += batch * num_threads;
			//cout<<k<<endl;
			memcpy(positionVecDaoE1, positionVecE1, PositionTotalE1 * dimensionWPE* sizeof(float));
			memcpy(positionVecDaoE2, positionVecE2, PositionTotalE2 * dimensionWPE* sizeof(float));
			memcpy(matrixW1PositionE1Dao, matrixW1PositionE1, dimensionC * dimensionWPE * window* sizeof(float));
			memcpy(matrixW1PositionE2Dao, matrixW1PositionE2, dimensionC * dimensionWPE * window* sizeof(float));
			memcpy(wordVecDao, wordVec, dimension * wordTotal * sizeof(float));

			memcpy(matrixW1Dao, matrixW1, sizeof(float) * dimensionC * dimension * window);
			memcpy(matrixB1Dao, matrixB1, sizeof(float) * 3 * dimensionC);
			memcpy(matrixRelationPrDao, matrixRelationPr, relationTotal * sizeof(float));				//add
			memcpy(matrixRelationDao, matrixRelation, 3 * dimensionC*relationTotal * sizeof(float));
			pthread_t *pt = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
			for (int a = 0; a < num_threads; a++)
				pthread_create(&pt[a], NULL, trainMode,  (void *)a);
			for (int a = 0; a < num_threads; a++)
			pthread_join(pt[a], NULL);
			free(pt);
			if (k%(npoch/5)==0)
			{
				cout<<"npoch:\t"<<k<<'/'<<npoch<<endl;
				time_end();
				time_begin();
				cout<<"score:\t"<<score-score1<<' '<<score_tmp<<endl;
				score1 = score;
			}
		}
		printf("Total Score:\t%f\n",score);
		printf("test\n");
		test();
		//if ((turn+1)%1==0) 
		rate=rate*reduce;
		//if (best_scorce > 0.8)
			//break;
	}
	cout<<"Train End"<<endl;
}

int main(int argc, char ** argv) {
	output_model = 1;
	logg = fopen("log.txt","w");
	cout<<"Init Begin."<<endl;
	init();
	cout<<"Init End."<<endl;
	train();
	fclose(logg);
}
