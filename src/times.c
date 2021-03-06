/*

PhyML:  a program that  computes maximum likelihood phylogenies from
DNA or AA homologous sequences.

Copyright (C) Stephane Guindon. Oct 2003 onward.

All parts of the source except where indicated are distributed under
the GNU public licence. See http://www.opensource.org for details.

*/


/* Routines for molecular clock trees and molecular dating */


#include "times.h"

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

#ifdef PHYTIME

int TIMES_main(int argc, char **argv)
{
  align **data;
  calign *cdata;
  option *io;
  t_tree *tree;
  int num_data_set;
  int num_tree,num_rand_tree;
  t_mod *mod;
  time_t t_beg,t_end;
  int r_seed;
  char *most_likely_tree;
  int i;
  int user_lk_approx;
  
#ifdef MPI
  int rc;
  rc = MPI_Init(&argc,&argv);
  if (rc != MPI_SUCCESS) 
    {
      PhyML_Printf("\n== Error starting MPI program. Terminating.\n");
      MPI_Abort(MPI_COMM_WORLD, rc);
    }
  MPI_Comm_size(MPI_COMM_WORLD,&Global_numTask);
  MPI_Comm_rank(MPI_COMM_WORLD,&Global_myRank);
#endif

#ifdef QUIET
  setvbuf(stdout,NULL,_IOFBF,2048);
#endif

  tree             = NULL;
  mod              = NULL;
  data             = NULL;
  most_likely_tree = NULL;

  io = (option *)Get_Input(argc,argv);

  r_seed = (io->r_seed < 0)?(time(NULL)):(io->r_seed);
  io->r_seed = r_seed;
  srand(r_seed); rand();
  PhyML_Printf("\n. Seed: %d\n",r_seed);
  PhyML_Printf("\n. Pid: %d\n",getpid());
  Make_Model_Complete(io->mod);
  mod = io->mod;
  if(io->in_tree == 2) Test_Multiple_Data_Set_Format(io);
  else io->n_trees = 1;
  
  if((io->n_data_sets > 1) && (io->n_trees > 1))
    {
      io->n_data_sets = MIN(io->n_trees,io->n_data_sets);
      io->n_trees     = MIN(io->n_trees,io->n_data_sets);
    }
  

  For(num_data_set,io->n_data_sets)
    {
      data = Get_Seq(io);

      if(data)
	{
	  if(io->n_data_sets > 1) PhyML_Printf("\n. Data set [#%d]\n",num_data_set+1);
	  PhyML_Printf("\n. Compressing sequences...\n");
	  cdata = Compact_Data(data,io);
	  Free_Seq(data,cdata->n_otu);
	  Check_Ambiguities(cdata,io->mod->io->datatype,io->state_len);

	  for(num_tree=(io->n_trees == 1)?(0):(num_data_set);num_tree < io->n_trees;num_tree++)
	    {
	      if(!io->mod->s_opt->random_input_tree) io->mod->s_opt->n_rand_starts = 1;

	      For(num_rand_tree,io->mod->s_opt->n_rand_starts)
		{
		  if((io->mod->s_opt->random_input_tree) && (io->mod->s_opt->topo_search != NNI_MOVE))
		    PhyML_Printf("\n. [Random start %3d/%3d]\n",num_rand_tree+1,io->mod->s_opt->n_rand_starts);


		  Init_Model(cdata,mod,io);

		  if(io->mod->use_m4mod) M4_Init_Model(mod->m4mod,cdata,mod);

		  /* A BioNJ tree is built here */
		  if(!io->in_tree) tree = Dist_And_BioNJ(cdata,mod,io);
		  /* A user-given tree is used here instead of BioNJ */
		  else             tree = Read_User_Tree(cdata,mod,io);


 		  if(io->fp_in_constraint_tree != NULL) 
		    {
		      io->cstr_tree        = Read_Tree_File_Phylip(io->fp_in_constraint_tree);		      
		      io->cstr_tree->rates = RATES_Make_Rate_Struct(io->cstr_tree->n_otu);
		      RATES_Init_Rate_Struct(io->cstr_tree->rates,io->rates,io->cstr_tree->n_otu);
		    }

		  if(!tree) continue;

		  if(!tree->n_root) 
		    {
		      PhyML_Printf("\n== Sorry, PhyTime requires a rooted tree as input.");
		      Exit("\n");      
		    }

		  time(&t_beg);
		  time(&(tree->t_beg));
		  tree->rates = RATES_Make_Rate_Struct(tree->n_otu);
		  RATES_Init_Rate_Struct(tree->rates,io->rates,tree->n_otu);

		  Update_Ancestors(tree->n_root,tree->n_root->v[2],tree);
		  Update_Ancestors(tree->n_root,tree->n_root->v[1],tree);		  

		  RATES_Fill_Lca_Table(tree);

		  tree->mod         = mod;
		  tree->io          = io;
		  tree->data        = cdata;
		  tree->n_pattern   = tree->data->crunch_len/tree->io->state_len;

                  Set_Both_Sides(YES,tree);

		  Prepare_Tree_For_Lk(tree);

		  /* Read node age priors */
		  Read_Clade_Priors(io->clade_list_file,tree);

		  /* Set upper and lower bounds for all node ages */
		  TIMES_Set_All_Node_Priors(tree);

		  /* Count the number of time slices */
		  TIMES_Get_Number_Of_Time_Slices(tree);
		  
                  TIMES_Label_Edges_With_Calibration_Intervals(tree);
                  tree->write_br_lens = NO;
                  PhyML_Printf("\n");
                  PhyML_Printf("\n. Input tree with calibration information ('almost' compatible with MCMCtree).\n");
                  PhyML_Printf("\n%s\n",Write_Tree(tree,YES));
                  tree->write_br_lens = YES;


		  /* Get_Edge_Binary_Coding_Number(tree); */
		  /* Exit("\n"); */

		  /* Print_CSeq_Select(stdout,NO,tree->data,tree); */
		  /* Exit("\n"); */

		  /* TIMES_Set_Root_Given_Tip_Dates(tree); */
		  /* int i; */
		  /* char *s; */
		  /* FILE *fp; */
		  /* For(i,2*tree->n_otu-2) tree->rates->cur_l[i] = 1.; */
		  /* s = Write_Tree(tree,NO); */
		  /* fp = fopen("rooted_tree","w"); */
		  /* fprintf(fp,"%s\n",s); */
		  /* fclose(fp); */
		  /* Exit("\n"); */


		  /* Work with log of branch lengths? */
		  if(tree->mod->log_l == YES) Log_Br_Len(tree);

		  if(io->mcmc->use_data == YES)
		    {
		      /* Force the exact likelihood score */
		      user_lk_approx = tree->io->lk_approx;
		      tree->io->lk_approx = EXACT;
		                            
                      /* printf("\n. Lk: %f",Lk(NULL,tree)); */
                      /* Exit("\n"); */

		      /* MLE for branch lengths */
		      Round_Optimize(tree,tree->data,ROUND_MAX);
		      
		      /* Set vector of mean branch lengths for the Normal approximation
			 of the likelihood */
		      RATES_Set_Mean_L(tree);
		      
		      /* Estimate the matrix of covariance for the Normal approximation of
			 the likelihood */
		      PhyML_Printf("\n");
		      PhyML_Printf("\n. Computing Hessian...");
		      tree->rates->bl_from_rt = 0;
		      Free(tree->rates->cov_l);
		      tree->rates->cov_l = Hessian_Seo(tree);
		      /* tree->rates->cov_l = Hessian_Log(tree); */
		      For(i,(2*tree->n_otu-3)*(2*tree->n_otu-3)) tree->rates->cov_l[i] *= -1.0;
		      if(!Iter_Matinv(tree->rates->cov_l,2*tree->n_otu-3,2*tree->n_otu-3,YES)) Exit("\n");
		      tree->rates->covdet = Matrix_Det(tree->rates->cov_l,2*tree->n_otu-3,YES);
		      For(i,(2*tree->n_otu-3)*(2*tree->n_otu-3)) tree->rates->invcov[i] = tree->rates->cov_l[i];
		      if(!Iter_Matinv(tree->rates->invcov,2*tree->n_otu-3,2*tree->n_otu-3,YES)) Exit("\n");
		      tree->rates->grad_l = Gradient(tree);
		      
		      
		      /* Pre-calculation of conditional variances to speed up calculations */
		      RATES_Bl_To_Ml(tree);
		      RATES_Get_Conditional_Variances(tree);
		      RATES_Get_All_Reg_Coeff(tree);
		      RATES_Get_Trip_Conditional_Variances(tree);
		      RATES_Get_All_Trip_Reg_Coeff(tree);
		      
		      Lk(NULL,tree);
		      PhyML_Printf("\n");
		      PhyML_Printf("\n. p(data|model) [exact ] ~ %.2f",tree->c_lnL);
		      
		      tree->io->lk_approx = NORMAL;
		      For(i,2*tree->n_otu-3) tree->rates->u_cur_l[i] = tree->rates->mean_l[i] ;
		      tree->c_lnL = Lk(NULL,tree);
		      PhyML_Printf("\n. p(data|model) [approx] ~ %.2f",tree->c_lnL);

		      tree->io->lk_approx = user_lk_approx;
		    }

		  tree->rates->model = io->rates->model;		  
		  PhyML_Printf("\n. Selected model '%s'",RATES_Get_Model_Name(io->rates->model));
		  if(tree->rates->model == GUINDON) tree->mod->gamma_mgf_bl = YES;
		  
		  tree->rates->bl_from_rt = YES;
		  
		  if(tree->io->cstr_tree) Find_Surviving_Edges_In_Small_Tree(tree,tree->io->cstr_tree);

		  time(&t_beg);
		  tree->mcmc = MCMC_Make_MCMC_Struct();
		  MCMC_Copy_MCMC_Struct(tree->io->mcmc,tree->mcmc,"phytime");
		  MCMC_Complete_MCMC(tree->mcmc,tree);
		  tree->mcmc->is_burnin = NO;
		  tree->mod->ras->sort_rate_classes = YES;
                  MCMC(tree);
		  MCMC_Close_MCMC(tree->mcmc);
		  MCMC_Free_MCMC(tree->mcmc);
                  Add_Root(tree->a_edges[0],tree);
		  Free_Tree_Pars(tree);
		  Free_Tree_Lk(tree);
		  Free_Tree(tree);
		}

	      break;
	    }
	  Free_Cseq(cdata);
	}
    }

  Free_Model(mod);

  if(io->fp_in_align)   fclose(io->fp_in_align);
  if(io->fp_in_tree)    fclose(io->fp_in_tree);
  if(io->fp_out_lk)     fclose(io->fp_out_lk);
  if(io->fp_out_tree)   fclose(io->fp_out_tree);
  if(io->fp_out_trees)  fclose(io->fp_out_trees);
  if(io->fp_out_stats)  fclose(io->fp_out_stats);

  Free(most_likely_tree);
  Free_Input(io);

  time(&t_end);
  Print_Time_Info(t_beg,t_end);

#ifdef MPI
  MPI_Finalize();
#endif

  return 0;
}

#endif

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


void TIMES_Least_Square_Node_Times(t_edge *e_root, t_tree *tree)
{

  /* Solve A.x = b, where x are the t_node time estimated
     under the least square criterion.

     A is a n x n matrix, with n being the number of
     nodes in a rooted tree (i.e. 2*n_otu-1).

   */

  phydbl *A, *b, *x;
  int n;
  int i,j;
  t_node *root;

  n = 2*tree->n_otu-1;
  
  A = (phydbl *)mCalloc(n*n,sizeof(phydbl));
  b = (phydbl *)mCalloc(n,  sizeof(phydbl));
  x = (phydbl *)mCalloc(n,  sizeof(phydbl));
    
  if(!tree->n_root && e_root) Add_Root(e_root,tree);
  else if(!e_root)            Add_Root(tree->a_edges[0],tree);
  
  root = tree->n_root;

  TIMES_Least_Square_Node_Times_Pre(root,root->v[2],A,b,n,tree);
  TIMES_Least_Square_Node_Times_Pre(root,root->v[1],A,b,n,tree);
  
  b[root->num] = tree->e_root->l->v/2.;
  
  A[root->num * n + root->num]       = 1.0;
  A[root->num * n + root->v[2]->num] = -.5;
  A[root->num * n + root->v[1]->num] = -.5;
    
  if(!Matinv(A, n, n,YES))
    {
      PhyML_Printf("\n== Err. in file %s at line %d (function '%s').\n",__FILE__,__LINE__,__FUNCTION__);
      Exit("\n");      
    }

  For(i,n) x[i] = .0;
  For(i,n) For(j,n) x[i] += A[i*n+j] * b[j];

  For(i,n-1) { tree->rates->nd_t[tree->a_nodes[i]->num] = -x[i]; }
  tree->rates->nd_t[root->num] = -x[n-1];
  tree->n_root->l[2] = tree->rates->nd_t[root->v[2]->num] - tree->rates->nd_t[root->num];
  tree->n_root->l[1] = tree->rates->nd_t[root->v[1]->num] - tree->rates->nd_t[root->num];
  ////////////////////////////////////////
  return;
  ////////////////////////////////////////

  /* Rescale the t_node times such that the time at the root
     is -100. This constraint implies that the clock rate
     is fixed to the actual tree length divided by the tree
     length measured in term of differences of t_node times */

  phydbl scale_f,time_tree_length,tree_length;

  scale_f = -100./tree->rates->nd_t[root->num];
  For(i,2*tree->n_otu-1) tree->rates->nd_t[i] *= scale_f;
  For(i,2*tree->n_otu-1) if(tree->rates->nd_t[i] > .0) tree->rates->nd_t[i] = .0;

  time_tree_length = 0.0;
  For(i,2*tree->n_otu-3)
    if(tree->a_edges[i] != tree->e_root)
      time_tree_length +=
	FABS(tree->rates->nd_t[tree->a_edges[i]->left->num] -
	     tree->rates->nd_t[tree->a_edges[i]->rght->num]);
  time_tree_length += FABS(tree->rates->nd_t[root->num] - tree->rates->nd_t[root->v[2]->num]);
  time_tree_length += FABS(tree->rates->nd_t[root->num] - tree->rates->nd_t[root->v[1]->num]);
  
  tree_length = 0.0;
  For(i,2*tree->n_otu-3) tree_length += tree->a_edges[i]->l->v;

  tree->rates->clock_r = tree_length / time_tree_length;

  Free(A);
  Free(b);
  Free(x);

}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


void TIMES_Least_Square_Node_Times_Pre(t_node *a, t_node *d, phydbl *A, phydbl *b, int n, t_tree *tree)
{
  if(d->tax)
    {
      A[d->num * n + d->num] = 1.;
      
      /* Set the time stamp at tip nodes to 0.0 */
/*       PhyML_Printf("\n. Tip t_node date set to 0"); */
      b[d->num] = 0.0;
      return;
    }
  else
    {
      int i;
      
 
      For(i,3)
	if((d->v[i] != a) && (d->b[i] != tree->e_root))
	  TIMES_Least_Square_Node_Times_Pre(d,d->v[i],A,b,n,tree);
      
      A[d->num * n + d->num] = 1.;
      b[d->num] = .0;
      For(i,3)
	{
	  A[d->num * n + d->v[i]->num] = -1./3.;
	  if(d->v[i] != a) b[d->num] += d->b[i]->l->v;
	  else             b[d->num] -= d->b[i]->l->v;
	}
      b[d->num] /= 3.;
    }
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


/* Adjust t_node times in order to have correct time stamp ranking with
 respect to the tree topology */

void TIMES_Adjust_Node_Times(t_tree *tree)
{
  TIMES_Adjust_Node_Times_Pre(tree->n_root->v[2],tree->n_root->v[1],tree);
  TIMES_Adjust_Node_Times_Pre(tree->n_root->v[1],tree->n_root->v[2],tree);

  if(tree->rates->nd_t[tree->n_root->num] > MIN(tree->rates->nd_t[tree->n_root->v[2]->num],
						tree->rates->nd_t[tree->n_root->v[1]->num]))
    {
      tree->rates->nd_t[tree->n_root->num] = MIN(tree->rates->nd_t[tree->n_root->v[2]->num],
						 tree->rates->nd_t[tree->n_root->v[1]->num]);
    }
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


void TIMES_Adjust_Node_Times_Pre(t_node *a, t_node *d, t_tree *tree)
{
  if(d->tax) return;
  else
    {
      int i;
      phydbl min_height;

      For(i,3)
	if((d->v[i] != a) && (d->b[i] != tree->e_root))
	  {
	    TIMES_Adjust_Node_Times_Pre(d,d->v[i],tree);
	  }

      min_height = 0.0;
      For(i,3)
	{
	  if((d->v[i] != a) && (d->b[i] != tree->e_root))
	    {
	      if(tree->rates->nd_t[d->v[i]->num] < min_height)
		{
		  min_height = tree->rates->nd_t[d->v[i]->num];
		}
	    }
	}

      if(tree->rates->nd_t[d->num] > min_height) tree->rates->nd_t[d->num] = min_height;

      if(tree->rates->nd_t[d->num] < -100.) tree->rates->nd_t[d->num] = -100.;

    }
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


  /* Multiply each time stamp at each internal 
     t_node by  'tree->time_stamp_mult'.
   */

void TIMES_Mult_Time_Stamps(t_tree *tree)
{
  int i;
  For(i,2*tree->n_otu-2) tree->rates->nd_t[tree->a_nodes[i]->num] *= FABS(tree->mod->s_opt->tree_size_mult);
  tree->rates->nd_t[tree->n_root->num] *= FABS(tree->mod->s_opt->tree_size_mult);
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

void TIMES_Print_Node_Times(t_node *a, t_node *d, t_tree *tree)
{
  t_edge *b;
  int i;
  
  b = NULL;
  For(i,3) if((d->v[i]) && (d->v[i] == a)) {b = d->b[i]; break;}

  PhyML_Printf("\n. (%3d %3d) a->t = %12f d->t = %12f (#=%12f) b->l->v = %12f [%12f;%12f]",
	       a->num,d->num,
	       tree->rates->nd_t[a->num],
	       tree->rates->nd_t[d->num],
	       tree->rates->nd_t[a->num]-tree->rates->nd_t[d->num],
	       (b)?(b->l->v):(-1.0),
	       tree->rates->t_prior_min[d->num],
	       tree->rates->t_prior_max[d->num]);
  if(d->tax) return;
  else
    {
      int i;
      For(i,3)
	if((d->v[i] != a) && (d->b[i] != tree->e_root))
	  TIMES_Print_Node_Times(d,d->v[i],tree);
    }
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

void TIMES_Set_All_Node_Priors(t_tree *tree)
{
  int i;
  phydbl min_prior;

  /* Set all t_prior_max values */
  TIMES_Set_All_Node_Priors_Bottom_Up(tree->n_root,tree->n_root->v[2],tree);
  TIMES_Set_All_Node_Priors_Bottom_Up(tree->n_root,tree->n_root->v[1],tree);

  tree->rates->t_prior_max[tree->n_root->num] = 
    MIN(tree->rates->t_prior_max[tree->n_root->num],
	MIN(tree->rates->t_prior_max[tree->n_root->v[2]->num],
	    tree->rates->t_prior_max[tree->n_root->v[1]->num]));


  /* Set all t_prior_min values */
  if(!tree->rates->t_has_prior[tree->n_root->num])
    {
      min_prior = 1.E+10;
      For(i,2*tree->n_otu-2)
	{
	  if(tree->rates->t_has_prior[i])
	    {
	      if(tree->rates->t_prior_min[i] < min_prior)
		min_prior = tree->rates->t_prior_min[i];
	    }
	}
      tree->rates->t_prior_min[tree->n_root->num] = 2.0 * min_prior;
      /* tree->rates->t_prior_min[tree->n_root->num] = 10. * min_prior; */
    }
  
  if(tree->rates->t_prior_min[tree->n_root->num] > 0.0)
    {
      PhyML_Printf("\n== Failed to set the lower bound for the root node.");
      PhyML_Printf("\n== Make sure at least one of the calibration interval");
      PhyML_Printf("\n== provides a lower bound.");
      Exit("\n");
    }


  TIMES_Set_All_Node_Priors_Top_Down(tree->n_root,tree->n_root->v[2],tree);
  TIMES_Set_All_Node_Priors_Top_Down(tree->n_root,tree->n_root->v[1],tree);

  Get_Node_Ranks(tree);
  TIMES_Set_Floor(tree);
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


void TIMES_Set_All_Node_Priors_Bottom_Up(t_node *a, t_node *d, t_tree *tree)
{
  int i;
  phydbl t_sup;
  
  if(d->tax) return;
  else 
    {
      t_node *v1, *v2; /* the two sons of d */

      For(i,3)
	{
	  if((d->v[i] != a) && (d->b[i] != tree->e_root))
	    {
	      TIMES_Set_All_Node_Priors_Bottom_Up(d,d->v[i],tree);	      
	    }
	}
      
      v1 = v2 = NULL;
      For(i,3) if((d->v[i] != a) && (d->b[i] != tree->e_root)) 
	{
	  if(!v1) v1 = d->v[i]; 
	  else    v2 = d->v[i];
	}
      
      if(tree->rates->t_has_prior[d->num] == YES)
	{
	  t_sup = MIN(tree->rates->t_prior_max[d->num],
		      MIN(tree->rates->t_prior_max[v1->num],
			  tree->rates->t_prior_max[v2->num]));

	  tree->rates->t_prior_max[d->num] = t_sup;

	  if(tree->rates->t_prior_max[d->num] < tree->rates->t_prior_min[d->num])
	    {
	      PhyML_Printf("\n== prior_min=%f prior_max=%f",tree->rates->t_prior_min[d->num],tree->rates->t_prior_max[d->num]);
	      PhyML_Printf("\n== Inconsistency in the prior settings detected at node %d",d->num);
	      PhyML_Printf("\n== Err. in file %s at line %d (function %s)\n\n",__FILE__,__LINE__,__FUNCTION__);
	      Warn_And_Exit("\n");
	    }
	}
      else
	{
	  tree->rates->t_prior_max[d->num] = 
	    MIN(tree->rates->t_prior_max[v1->num],
		tree->rates->t_prior_max[v2->num]);
	}
    }
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


void TIMES_Set_All_Node_Priors_Top_Down(t_node *a, t_node *d, t_tree *tree)
{
  if(d->tax) return;
  else
    {
      int i;      
      
      if(tree->rates->t_has_prior[d->num] == YES)
	{
	  tree->rates->t_prior_min[d->num] = MAX(tree->rates->t_prior_min[d->num],tree->rates->t_prior_min[a->num]);
	  
	  if(tree->rates->t_prior_max[d->num] < tree->rates->t_prior_min[d->num])
	    {
	      PhyML_Printf("\n== prior_min=%f prior_max=%f",tree->rates->t_prior_min[d->num],tree->rates->t_prior_max[d->num]);
	      PhyML_Printf("\n== Inconsistency in the prior settings detected at t_node %d",d->num);
	      PhyML_Printf("\n== Err. in file %s at line %d (function %s)\n\n",__FILE__,__LINE__,__FUNCTION__);
	      Warn_And_Exit("\n");
	    }
	}
      else
	{
	  tree->rates->t_prior_min[d->num] = tree->rates->t_prior_min[a->num];
	}
            
      For(i,3)
	{
	  if((d->v[i] != a) && (d->b[i] != tree->e_root))
	    {
	      TIMES_Set_All_Node_Priors_Top_Down(d,d->v[i],tree);
	    }
	}
    }
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


void TIMES_Set_Floor(t_tree *tree)
{
  TIMES_Set_Floor_Post(tree->n_root,tree->n_root->v[2],tree);
  TIMES_Set_Floor_Post(tree->n_root,tree->n_root->v[1],tree);
  tree->rates->t_floor[tree->n_root->num] = MIN(tree->rates->t_floor[tree->n_root->v[2]->num],
						tree->rates->t_floor[tree->n_root->v[1]->num]);
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


void TIMES_Set_Floor_Post(t_node *a, t_node *d, t_tree *tree)
{
  if(d->tax)
    {
      tree->rates->t_floor[d->num] = tree->rates->nd_t[d->num];
      d->rank_max = d->rank;
      return;
    }
  else
    {
      int i;
      t_node *v1,*v2;

      v1 = v2 = NULL;
      For(i,3)
	{
	  if(d->v[i] != a && d->b[i] != tree->e_root)
	    {
	      TIMES_Set_Floor_Post(d,d->v[i],tree);
	      if(!v1) v1 = d->v[i];
	      else    v2 = d->v[i];
	    }
	}
      tree->rates->t_floor[d->num] = MIN(tree->rates->t_floor[v1->num],
					 tree->rates->t_floor[v2->num]);

      if(tree->rates->t_floor[v1->num] < tree->rates->t_floor[v2->num])
	{
	  d->rank_max = v1->rank_max;
	}
      else if(tree->rates->t_floor[v2->num] < tree->rates->t_floor[v1->num])
	{
	  d->rank_max = v2->rank_max;
	}
      else
	{
	  d->rank_max = MAX(v1->rank_max,v2->rank_max);
	}
    }
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

/* Does it work for serial samples? */
phydbl TIMES_Log_Conditional_Uniform_Density(t_tree *tree)
{
  phydbl min,max;
  phydbl dens;
  int i;

  min = tree->rates->nd_t[tree->n_root->num];

  dens = 0.0;
  For(i,2*tree->n_otu-1)
    {
      if((tree->a_nodes[i]->tax == NO) && (tree->a_nodes[i] != tree->n_root))
	{
	  max = tree->rates->t_floor[i];

	  dens += LOG(Dorder_Unif(tree->rates->nd_t[i],
				  tree->a_nodes[i]->rank-1,
				  tree->a_nodes[i]->rank_max-2,
				  min,max));
	}
    }
  return dens;
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// Returns the marginal density of tree height assuming the
// Yule model of speciation. 
phydbl TIMES_Lk_Yule_Root_Marginal(t_tree *tree)
{
  int n;
  int j;
  t_node *nd;
  phydbl *t,*ts;
  phydbl lbda;
  phydbl T;

  lbda = tree->rates->birth_rate;
  t    = tree->rates->nd_t;
  ts   = tree->rates->time_slice_lims;
  T    = ts[0] - t[tree->n_root->num];

  n = 0;
  nd = NULL;
  For(j,2*tree->n_otu-2) 
    {
      nd = tree->a_nodes[j];

      if((t[nd->num] > ts[0] && t[nd->anc->num] < ts[0]) || // lineage that is crossing ts[0]
	 (nd->tax == YES && Are_Equal(t[nd->num],ts[0],1.E-6) == YES)) // tip that is lying on ts[0]
	n++;
    }

  return LnGamma(n+1) + LOG(lbda) - 2.*lbda*T + (n-2.)*LOG(1. - EXP(-lbda*T));
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// Returns the joint density of internal node heights assuming
// the Yule model of speciation.
phydbl TIMES_Lk_Yule_Joint(t_tree *tree)
{
  int i,j;
  phydbl loglk;
  phydbl *t;
  phydbl dt;
  int n; // number of lineages at a given time point
  phydbl lbda;
  t_node *nd;
  phydbl *ts;
  int *tr;
  phydbl top_t;
  short int *interrupted;
  phydbl sumdt;

  interrupted = (short int *)mCalloc(tree->n_otu,sizeof(short int));

  t = tree->rates->nd_t;
  ts = tree->rates->time_slice_lims;
  tr = tree->rates->t_rank;
  lbda = tree->rates->birth_rate;

  TIMES_Update_Node_Ordering(tree);

  For(j,tree->n_otu) interrupted[j] = NO;

  loglk = .0;

  sumdt = .0;
  n = 1;
  For(i,2*tree->n_otu-2) // t[tr[0]] is the time of the oldest node, t[tr[1]], the second oldest and so on...
    {

      For(j,tree->n_otu)
	if((t[j] < t[tr[i]]) && (interrupted[j] == NO)) 
	  {
	    interrupted[j] = YES;
	    n--; // How many lineages have stopped above t[tr[i]]?
	  }
      
      top_t = t[tr[i+1]];
      dt = top_t - t[tr[i]];
      sumdt += dt;

      /* printf("\n. %d node up=%d [%f] node do=%d [%f] dt=%f",i,tr[i],t[tr[i]],tr[i+1],t[tr[i+1]],dt); */

      if(n<1)
	{
	  PhyML_Printf("\n== i=%d tr[i]=%f",i,t[tr[i]]);
	  PhyML_Printf("\n== Err. in file %s at line %d\n",__FILE__,__LINE__);
	  Exit("\n");
	}

      if(dt > 1.E-10) loglk += LOG((n+1)*lbda) - (n+1)*lbda*dt;
      n++;      
    }

  /* printf("\n. sumdt = %f th=%f",sumdt,tree->rates->nd_t[tree->n_root->num]); */
  /* printf("\n0 loglk = %f",loglk); */

  For(i,tree->rates->n_time_slices-1)
    {
      n = 0;
      dt = 0.;
      For(j,2*tree->n_otu-2)
  	{
  	  nd = tree->a_nodes[j];
  	  if(t[nd->num] > ts[i] && t[nd->anc->num] < ts[i]) // How many lineages are crossing this time slice limit?
  	    {
  	      n++;
  	      if(t[nd->num] < dt) dt = t[nd->num]; // take the oldest node younger than the time slice
  	    }
  	}
      dt -= ts[i];
      loglk += LOG(n*lbda) - n*lbda*dt;
    }

  /* printf("\n1 loglk = %f",loglk); */

  Free(interrupted);

  return loglk;
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// Returns the conditional density of internal node heights 
// given the tree height under the Yule model. Uses the order
// statistics 'simplification' as described in Yang and Rannala, 2005. 
phydbl TIMES_Lk_Yule_Order(t_tree *tree)
{
  int j;
  phydbl *t,*tf;
  t_node *n;
  phydbl loglk;
  phydbl loglbda;
  phydbl lbda;
  phydbl *tp_min,*tp_max;
  phydbl lower_bound,upper_bound;
  /* phydbl root_height; */

  tp_min = tree->rates->t_prior_min;
  tp_max = tree->rates->t_prior_max;
  tf = tree->rates->t_floor;
  t  = tree->rates->nd_t;
  n = NULL;
  loglbda = LOG(tree->rates->birth_rate);
  lbda = tree->rates->birth_rate;
  lower_bound = -1.;
  upper_bound = -1.;
  /* root_height = FABS(tree->rates->nd_t[tree->n_root->num]); */

  /*! Adapted from  Equation (6) in T. Stadler's Systematic Biology, 2012 paper with
      sampling fraction set to 1 and death rate set to 0. Dropped the 1/(n-1) scaling 
      factor. */

  /* loglk = 0.0; */
  /* For(j,2*tree->n_otu-2) */
  /*   { */
  /*     n = tree->a_nodes[j]; */
  /*     lower_bound = MAX(FABS(tf[j]),FABS(tp_max[j])); */
  /*     upper_bound = MIN(FABS(t[tree->n_root->num]),FABS(tp_min[j])); */

  /*     if(n->tax == NO) */
  /*       { */
  /*         loglk  += (loglbda - lbda * FABS(t[j])); */
  /*         /\* loglk -= LOG(EXP(-lbda*lower_bound) - EXP(-lbda*upper_bound)); // incorporate calibration boundaries here. *\/ */
  /*       } */
  /*   } */

  
  /*! Adapted from  Equation (7) in T. Stadler's Systematic Biology, 2012 paper with
      sampling fraction set to 1 and death rate set to 0. */

  // Check that each node is within its calibration-derived interval
  For(j,2*tree->n_otu-1) if(t[j] < tp_min[j] || t[j] > tp_max[j]) return(-INFINITY);

  loglk = 0.0;
  For(j,2*tree->n_otu-2)
    {
      n = tree->a_nodes[j];
      lower_bound = MAX(FABS(tf[j]),FABS(tp_max[j]));
      upper_bound = FABS(tp_min[j]);
      
      if(n->tax == NO)
        {
          loglk  += (loglbda - lbda * FABS(t[j]));
          loglk -= LOG(EXP(-lbda*lower_bound) - EXP(-lbda*upper_bound)); // incorporate calibration boundaries here.    
        }
      
      if(isinf(loglk) || isnan(loglk))
        {
          /* PhyML_Printf("\n. Lower bound: %f",lower_bound); */
          /* PhyML_Printf("\n. Upper bound: %f",upper_bound); */
          /* PhyML_Printf("\n. tf: %f tp_max: %f tp_min: %f ",tf[j],tp_max[j],tp_min[j]); */
          /* PhyML_Printf("\n. exp1: %f",EXP(-lbda*lower_bound)); */
          /* PhyML_Printf("\n. exp2: %f",EXP(-lbda*upper_bound)); */
          /* PhyML_Printf("\n. diff: %f",EXP(-lbda*lower_bound) - EXP(-lbda*upper_bound)); */
          /* Exit("\n"); */
          return(-INFINITY);
        }      
    }

  lower_bound = MAX(FABS(tf[tree->n_root->num]),FABS(tp_max[tree->n_root->num]));
  upper_bound = FABS(tp_min[tree->n_root->num]);
  loglk += LOG(2) + loglbda - 2.*lbda * FABS(t[tree->n_root->num]);
  loglk -= LOG(EXP(-2.*lbda*lower_bound) - EXP(-2.*lbda*upper_bound));

  if(isinf(loglk) || isnan(loglk))
    {
      /* PhyML_Printf("\n. * Lower bound: %f",lower_bound); */
      /* PhyML_Printf("\n. * Upper bound: %f",upper_bound); */
      /* PhyML_Printf("\n. * tf: %f tp_max: %f tp_min: %f",tf[tree->n_root->num],tp_max[tree->n_root->num],tp_min[tree->n_root->num]); */
      /* PhyML_Printf("\n. * exp1: %f",EXP(-2.*lbda*lower_bound)); */
      /* PhyML_Printf("\n. * exp2: %f",EXP(-2.*lbda*upper_bound)); */
      /* PhyML_Printf("\n. * diff: %f",EXP(-2.*lbda*lower_bound) - EXP(-2.*lbda*upper_bound)); */
      /* Exit("\n"); */
      return(-INFINITY);
    }


  return(loglk);
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

phydbl TIMES_Lk_Times(t_tree *tree)
{
  
#ifdef PHYTIME
  tree->rates->c_lnL_times =  TIMES_Lk_Yule_Order(tree);
#elif defined(DATE)
  tree->rates->c_lnL_times =  TIMES_Lk_Birth_Death(tree);
#endif

  return(tree->rates->c_lnL_times);
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


void TIMES_Lk_Times_Trav(t_node *a, t_node *d, phydbl lim_inf, phydbl lim_sup, phydbl *logdens, t_tree *tree)
{
  int i;
  
  if(!d->tax)
    {
      /* if(tree->rates->nd_t[d->num] > lim_sup) */
      /* 	{ */
      /* 	  lim_inf = lim_sup; */
      /* 	  lim_sup = 0.0; */
      /* 	  For(i,2*tree->n_otu-2) */
      /* 	    if((tree->rates->t_floor[i] < lim_sup) && (tree->rates->t_floor[i] > tree->rates->nd_t[d->num])) */
      /* 	      lim_sup = tree->rates->t_floor[i]; */
      /* 	} */
      
      /* if(tree->rates->nd_t[d->num] < lim_inf || tree->rates->nd_t[d->num] > lim_sup) */
      /* 	{ */
      /* 	  PhyML_Printf("\n. nd_t = %f lim_inf = %f lim_sup = %f",tree->rates->nd_t[d->num],lim_inf,lim_sup); */
      /* 	  PhyML_Printf("\n. Err in file %s at line %d\n",__FILE__,__LINE__); */
      /* 	  Exit("\n"); */
      /* 	} */
  
      lim_inf = tree->rates->nd_t[tree->n_root->num];
      lim_sup = tree->rates->t_floor[d->num];
      
      *logdens = *logdens + LOG(lim_sup - lim_inf);   
    }
  
  if(d->tax == YES) return;
  else
    {      
      For(i,3)
	{
	  if(d->v[i] != a && d->b[i] != tree->e_root)
	    {
	      TIMES_Lk_Times_Trav(d,d->v[i],lim_inf,lim_sup,logdens,tree);
	    }
	}
    }
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


phydbl TIMES_Log_Number_Of_Ranked_Labelled_Histories(t_node *root, int per_slice, t_tree *tree)
{
  int i;
  phydbl logn;
  t_node *v1,*v2;
  int n1,n2;
  
  TIMES_Update_Curr_Slice(tree);

  logn = .0;
  v1 = v2 = NULL;
  if(root == tree->n_root)
    {
      TIMES_Log_Number_Of_Ranked_Labelled_Histories_Post(root,root->v[2],per_slice,&logn,tree);
      TIMES_Log_Number_Of_Ranked_Labelled_Histories_Post(root,root->v[1],per_slice,&logn,tree);
      v1 = root->v[2];
      v2 = root->v[1];
    }
  else
    {
      For(i,3)
	{
	  if(root->v[i] != root->anc && root->b[i] != tree->e_root)
	    {
	      TIMES_Log_Number_Of_Ranked_Labelled_Histories_Post(root,root->v[i],per_slice,&logn,tree);
	      if(!v1) v1 = root->v[i];
	      else    v2 = root->v[i];
	    }
	}
    }

 
  if(per_slice == NO)
    {
      n1 = tree->rates->n_tips_below[v1->num];
      n2 = tree->rates->n_tips_below[v2->num];
    }
  else
    {
      if(tree->rates->curr_slice[v1->num] == tree->rates->curr_slice[root->num])
  	n1 = tree->rates->n_tips_below[v1->num];
      else
  	n1 = 1;
      
      if(tree->rates->curr_slice[v2->num] == tree->rates->curr_slice[root->num])
  	n2 = tree->rates->n_tips_below[v2->num];
      else
  	n2 = 1;
    }

  tree->rates->n_tips_below[root->num] = n1+n2;

  logn += Factln(n1+n2-2) - Factln(n1-1) - Factln(n2-1);

  return(logn);
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


void TIMES_Log_Number_Of_Ranked_Labelled_Histories_Post(t_node *a, t_node *d, int per_slice, phydbl *logn, t_tree *tree)
{
  if(d->tax == YES) 
    {
      tree->rates->n_tips_below[d->num] = 1;
      return;
    }
  else
    {
      int i,n1,n2;
      t_node *v1, *v2;

      For(i,3)
	{
	  if(d->v[i] != a && d->b[i] != tree->e_root)
	    {
	      TIMES_Log_Number_Of_Ranked_Labelled_Histories_Post(d,d->v[i],per_slice,logn,tree);
	    }
	}

      v1 = v2 = NULL;
      For(i,3)
	{
	  if(d->v[i] != a && d->b[i] != tree->e_root)
	    {
	      if(v1 == NULL) {v1 = d->v[i];}
	      else           {v2 = d->v[i];}
	    }
	}


      if(per_slice == NO)
	{
	  n1 = tree->rates->n_tips_below[v1->num];
	  n2 = tree->rates->n_tips_below[v2->num];
	}
      else
	{
	  if(tree->rates->curr_slice[v1->num] == tree->rates->curr_slice[d->num])
	    n1 = tree->rates->n_tips_below[v1->num];
	  else
	    n1 = 1;

	  if(tree->rates->curr_slice[v2->num] == tree->rates->curr_slice[d->num])
	    n2 = tree->rates->n_tips_below[v2->num];
	  else
	    n2 = 1;
	}

      tree->rates->n_tips_below[d->num] = n1+n2;

      (*logn) += Factln(n1+n2-2) - Factln(n1-1) - Factln(n2-1);
    }
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


void TIMES_Update_Curr_Slice(t_tree *tree)
{
  int i,j;

  For(i,2*tree->n_otu-1)
    {
      For(j,tree->rates->n_time_slices)
	{
	  if(!(tree->rates->nd_t[i] > tree->rates->time_slice_lims[j])) break;
	}
      tree->rates->curr_slice[i] = j;

      /* PhyML_Printf("\n. Node %3d [%12f] is in slice %3d.",i,tree->rates->nd_t[i],j); */
    }
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


phydbl TIMES_Lk_Uniform_Core(t_tree *tree)
{  
  phydbl logn;

  logn = TIMES_Log_Number_Of_Ranked_Labelled_Histories(tree->n_root,YES,tree);

  tree->rates->c_lnL_times = 0.0;
  TIMES_Lk_Uniform_Post(tree->n_root,tree->n_root->v[2],tree);
  TIMES_Lk_Uniform_Post(tree->n_root,tree->n_root->v[1],tree);

  /* printf("\n. ^ %f %f %f", */
  /* 	 (phydbl)(tree->rates->n_tips_below[tree->n_root->num]-2.), */
  /* 	 LOG(tree->rates->time_slice_lims[tree->rates->curr_slice[tree->n_root->num]] - */
  /* 	     tree->rates->nd_t[tree->n_root->num]), */
  /* 	 (phydbl)(tree->rates->n_tips_below[tree->n_root->num]-2.) * */
  /* 	 LOG(tree->rates->time_slice_lims[tree->rates->curr_slice[tree->n_root->num]] - */
  /* 	     tree->rates->nd_t[tree->n_root->num])); */

  tree->rates->c_lnL_times +=
    Factln(tree->rates->n_tips_below[tree->n_root->num]-2.) -
    (phydbl)(tree->rates->n_tips_below[tree->n_root->num]-2.) *
    LOG(tree->rates->time_slice_lims[tree->rates->curr_slice[tree->n_root->num]] -
  	tree->rates->nd_t[tree->n_root->num]);
  
  tree->rates->c_lnL_times -= logn;
  
  return(tree->rates->c_lnL_times);
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


void TIMES_Get_Number_Of_Time_Slices(t_tree *tree)
{
  int i;

  tree->rates->n_time_slices=0;
  TIMES_Get_Number_Of_Time_Slices_Post(tree->n_root,tree->n_root->v[2],tree);
  TIMES_Get_Number_Of_Time_Slices_Post(tree->n_root,tree->n_root->v[1],tree);
  Qksort(tree->rates->time_slice_lims,NULL,0,tree->rates->n_time_slices-1);

  if(tree->rates->n_time_slices > 1)
    {
      PhyML_Printf("\n");
      PhyML_Printf("\n. Sequences were collected at %d different time points.",tree->rates->n_time_slices);
      For(i,tree->rates->n_time_slices) printf("\n+ [%3d] time point @ %12f ",i+1,tree->rates->time_slice_lims[i]);
    }
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


void TIMES_Get_Number_Of_Time_Slices_Post(t_node *a, t_node *d, t_tree *tree)
{
  int i;

  if(d->tax == YES)
    {
      For(i,tree->rates->n_time_slices) 
	if(Are_Equal(tree->rates->t_floor[d->num],tree->rates->time_slice_lims[i],1.E-6)) break;

      if(i == tree->rates->n_time_slices) 
	{
	  tree->rates->time_slice_lims[i] = tree->rates->t_floor[d->num];
	  tree->rates->n_time_slices++;
	}
      return;
    }
  else
    {
      For(i,3)
	if(d->v[i] != a && d->b[i] != tree->e_root)
	  TIMES_Get_Number_Of_Time_Slices_Post(d,d->v[i],tree);
    }
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


void TIMES_Get_N_Slice_Spans(t_tree *tree)
{
  int i,j;

  For(i,2*tree->n_otu-2)
    {
      if(tree->a_nodes[i]->tax == NO)
	{
	  For(j,tree->rates->n_time_slices)
	    {
	      if(Are_Equal(tree->rates->t_floor[i],tree->rates->time_slice_lims[j],1.E-6))
		{
		  tree->rates->n_time_slice_spans[i] = j+1;
		  /* PhyML_Printf("\n. Node %3d spans %3d slices [%12f].", */
		  /* 	       i+1, */
		  /* 	       tree->rates->n_slice_spans[i], */
		  /* 	       tree->rates->t_floor[i]); */
		  break;
		}
	    }
	}
    }
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


void TIMES_Lk_Uniform_Post(t_node *a, t_node *d, t_tree *tree)
{
  if(d->tax == YES) return;
  else
    {
      int i;

      For(i,3)
	{
	  if(d->v[i] != a && d->b[i] != tree->e_root)
	    {
	      TIMES_Lk_Uniform_Post(d,d->v[i],tree);
	    }
	}
      
      if(tree->rates->curr_slice[a->num] != tree->rates->curr_slice[d->num])
	{
	  tree->rates->c_lnL_times += 
	    Factln(tree->rates->n_tips_below[d->num]-1.) - 
	    (phydbl)(tree->rates->n_tips_below[d->num]-1.) *
	    LOG(tree->rates->time_slice_lims[tree->rates->curr_slice[d->num]] -
		tree->rates->nd_t[d->num]);
	}
    }
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

/* Set the root position so that most of the taxa in the outgroup 
   correspond to the most ancient time point.
*/
void TIMES_Set_Root_Given_Tip_Dates(t_tree *tree)
{
  int i,j;
  t_node *left,*rght;
  int n_left_in, n_left_out;
  int n_rght_in, n_rght_out;
  t_edge *b,*best;
  phydbl eps,score,max_score;
  
  Free_Bip(tree);
  Alloc_Bip(tree);
  Get_Bip(tree->a_nodes[0],tree->a_nodes[0]->v[0],tree);
  
  left = rght = NULL;
  b = best = NULL;
  n_left_in = n_left_out = -1;
  n_rght_in = n_rght_out = -1;
  eps = 1.E-6;
  score = max_score = -1.;

  For(i,2*tree->n_otu-3)
    {
      left = tree->a_edges[i]->left;
      rght = tree->a_edges[i]->rght;
      b    = tree->a_edges[i];

      n_left_in = 0;
      For(j,left->bip_size[b->l_r]) 
	if(FABS(tree->rates->nd_t[left->bip_node[b->l_r][j]->num] - tree->rates->time_slice_lims[0]) < eps)
	  n_left_in++;
      
      n_left_out = left->bip_size[b->l_r]-n_left_in;
      
      n_rght_in = 0;
      For(j,rght->bip_size[b->r_l]) 
	if(FABS(tree->rates->nd_t[rght->bip_node[b->r_l][j]->num] - tree->rates->time_slice_lims[0]) < eps)
	  n_rght_in++;

      n_rght_out = rght->bip_size[b->r_l]-n_rght_in;


      /* score = POW((phydbl)(n_left_in)/(phydbl)(n_left_in+n_left_out)- */
      /* 		  (phydbl)(n_rght_in)/(phydbl)(n_rght_in+n_rght_out),2); */
      /* score = (phydbl)(n_left_in * n_rght_out + eps)/(n_left_out * n_rght_in + eps); */
      /* score = (phydbl)(n_left_in * n_rght_out + eps); */
      score = FABS((phydbl)((n_left_in+1.) * (n_rght_out+1.)) - (phydbl)((n_left_out+1.) * (n_rght_in+1.)));
      
      if(score > max_score)
	{
	  max_score = score;
	  best = b;
	}
    }
  
  Add_Root(best,tree);
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


void Get_Survival_Duration(t_tree *tree)
{
  Get_Survival_Duration_Post(tree->n_root,tree->n_root->v[2],tree);
  Get_Survival_Duration_Post(tree->n_root,tree->n_root->v[1],tree);
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


void Get_Survival_Duration_Post(t_node *a, t_node *d, t_tree *tree)
{
  if(d->tax)
    {
      tree->rates->survival_dur[d->num] = tree->rates->nd_t[d->num];
      return;
    }
  else
    {
      int i;
      t_node *v1, *v2;

      For(i,3)
	if(d->v[i] != a && d->b[i] != tree->e_root)
	  Get_Survival_Duration_Post(d,d->v[i],tree);
      
      v1 = v2 = NULL;
      For(i,3)
	{
	  if(d->v[i] != a && d->b[i] != tree->e_root)
	    {
	      if(!v1) v1 = d->v[i];
	      else    v2 = d->v[i];
	    }
	}

      tree->rates->survival_dur[d->num] = MAX(tree->rates->survival_dur[v1->num],
					      tree->rates->survival_dur[v2->num]);
    }
}


//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

/* Update the ranking of node heights. Use bubble sort algorithm */

void TIMES_Update_Node_Ordering(t_tree *tree)
{
  int buff;
  int i;
  phydbl *t;
  int swap = NO;

  For(i,2*tree->n_otu-1) tree->rates->t_rank[i] = i;

  t = tree->rates->nd_t;

  do
    {
      swap = NO;
      For(i,2*tree->n_otu-2)
	{
	  if(t[tree->rates->t_rank[i]] > t[tree->rates->t_rank[i+1]]) // Sort in ascending order
	    {
	      swap = YES;
	      buff                     = tree->rates->t_rank[i];
	      tree->rates->t_rank[i]   = tree->rates->t_rank[i+1];
	      tree->rates->t_rank[i+1] = buff;
	    }	    
	}
    }
  while(swap == YES);

  /* For(i,2*tree->n_otu-1) PhyML_Printf("\n. node %3d time: %12f", */
  /*                                     tree->rates->t_rank[i], */
  /*                                     tree->rates->nd_t[tree->rates->t_rank[i]]); */
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

void TIMES_Label_Edges_With_Calibration_Intervals(t_tree *tree)
{
  char *s;
  int i;

  s = (char *)mCalloc(T_MAX_LINE,sizeof(char));
  
  tree->write_labels = YES;

  For(i,2*tree->n_otu-2)
    {
      if(tree->a_nodes[i]->tax == NO)
        {
          if(tree->rates->t_has_prior[i] == YES && tree->a_nodes[i]->b[0] != tree->e_root)
            {
              tree->a_nodes[i]->b[0]->n_labels = 1;
              Make_New_Edge_Label(tree->a_nodes[i]->b[0]);
              sprintf(s,"'>%f<%f'",FABS(tree->rates->t_prior_max[i])/100.,FABS(tree->rates->t_prior_min[i])/100.);
              tree->a_nodes[i]->b[0]->labels[0] = (char *)mCalloc(strlen(s)+1,sizeof(char));
              strcpy(tree->a_nodes[i]->b[0]->labels[0],s);
            }
        }
    }
  
  Free(s);

}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

void TIMES_Set_Calibration(t_tree *tree)
{
  t_cal *cal;
  int i;

  For(i,2*tree->n_otu-1)
    {
      tree->rates->t_has_prior[i] = NO;
      tree->rates->t_prior_min[i] = BIG;
      tree->rates->t_prior_max[i] = BIG; 
   }

  cal = tree->rates->a_cal[0];
  while(cal)
    {
      /* if(cal->is_active == YES) */
      /*   { */
          /* tree->rates->t_has_prior[cal->node_num] = YES; */
          /* tree->rates->t_prior_min[cal->node_num] = cal->lower; */
          /* tree->rates->t_prior_max[cal->node_num] = cal->upper;           */
        /* } */
      cal = cal->next;
    }

  TIMES_Set_All_Node_Priors(tree);
}



//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////


void TIMES_Record_Prior_Times(t_tree *tree)
{
  int i;
  For(i,2*tree->n_otu-1) 
    {
      tree->rates->t_prior_min_ori[i] = tree->rates->t_prior_min[i];
      tree->rates->t_prior_max_ori[i] = tree->rates->t_prior_max[i];
    }
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

void TIMES_Reset_Prior_Times(t_tree *tree)
{
  int i;
  For(i,2*tree->n_otu-1) 
    {
      tree->rates->t_prior_min[i] = tree->rates->t_prior_min_ori[i];
      tree->rates->t_prior_max[i] = tree->rates->t_prior_max_ori[i];
     }
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// Returns the conditional density of internal node heights 
// given the tree height under the Yule model. Uses the order
// statistics 'simplification' as described in Yang and Rannala, 2005. 
phydbl TIMES_Lk_Yule_Order_Root_Cond(t_tree *tree)
{
  int j;
  phydbl *t,*tf;
  t_node *n;
  phydbl loglk;
  phydbl loglbda;
  phydbl lbda;
  phydbl *tp_min,*tp_max;
  phydbl lower_bound,upper_bound;
  phydbl root_height;

  tp_min = tree->rates->t_prior_min;
  tp_max = tree->rates->t_prior_max;
  tf = tree->rates->t_floor;
  t  = tree->rates->nd_t;
  n = NULL;
  loglbda = LOG(tree->rates->birth_rate);
  lbda = tree->rates->birth_rate;
  lower_bound = -1.;
  upper_bound = -1.;
  root_height = FABS(tree->rates->nd_t[tree->n_root->num]);

  /*! Adapted from  Equation (6) in T. Stadler's Systematic Biology, 2012 paper with
      sampling fraction set to 1 and death rate set to 0. Dropped the 1/(n-1) scaling 
      factor. */

  /* loglk = 0.0; */
  /* For(j,2*tree->n_otu-2) */
  /*   { */
  /*     n = tree->a_nodes[j]; */
  /*     lower_bound = MAX(FABS(tf[j]),FABS(tp_max[j])); */
  /*     upper_bound = MIN(FABS(t[tree->n_root->num]),FABS(tp_min[j])); */

  /*     if(n->tax == NO) */
  /*       { */
  /*         loglk  += (loglbda - lbda * FABS(t[j])); */
  /*         /\* loglk -= LOG(EXP(-lbda*lower_bound) - EXP(-lbda*upper_bound)); // incorporate calibration boundaries here. *\/ */
  /*       } */
  /*   } */

  
  /*! Adapted from  Equation (7) in T. Stadler's Systematic Biology, 2012 paper with
      sampling fraction set to 1 and death rate set to 0. */

  // Check that each node is within its calibration-derived interval
  For(j,2*tree->n_otu-1) if(t[j] < tp_min[j] || t[j] > tp_max[j]) return(-INFINITY);

  loglk = 0.0;
  For(j,2*tree->n_otu-2)
    {
      n = tree->a_nodes[j];
      lower_bound = MAX(FABS(tf[j]),FABS(tp_max[j]));
      upper_bound = MIN(FABS(tp_min[j]),root_height);
      
      if(n->tax == NO)
        {
          loglk  += (loglbda - lbda * FABS(t[j]));
          loglk -= LOG(EXP(-lbda*lower_bound) - EXP(-lbda*upper_bound)); // incorporate calibration boundaries here.    
        }

        if(isinf(loglk) || isnan(loglk))
          {
            PhyML_Printf("\n. Lower bound: %f",lower_bound);
            PhyML_Printf("\n. Upper bound: %f",upper_bound);
            PhyML_Printf("\n. tf: %f tp_max: %f tp_min: %f root: %f",tf[j],tp_max[j],tp_min[j],root_height);
            Exit("\n");
          }

    }

  /* lower_bound = MAX(FABS(tf[tree->n_root->num]),FABS(tp_max[tree->n_root->num])); */
  /* upper_bound = FABS(tp_min[tree->n_root->num]); */
  /* loglk += LOG(2) + loglbda - 2.*lbda * FABS(t[tree->n_root->num]); */
  /* loglk -= LOG(EXP(-2.*lbda*lower_bound) - EXP(-2.*lbda*upper_bound)); */


  return(loglk);
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// Log of prob density of internal node ages conditional on tree height, under
// the birth death process with complete sampling.
phydbl TIMES_Lk_Birth_Death(t_tree *tree)
{
  int i;
  phydbl lnL;

  lnL = 0.0;

  // Normalizing factor. Need to call this function first
  // so that t_prior_min/max are up to date for what's next
  /* lnL -= LOG(DATE_J_Sum_Product(tree)); */

  For(i,2*tree->n_otu-1)
    if(tree->a_nodes[i]->tax == NO && tree->a_nodes[i] != tree->n_root)
      {
        lnL += TIMES_Lk_Birth_Death_One_Node(tree->rates->nd_t[i],
                                             tree->rates->t_prior_min[i],
                                             tree->rates->t_prior_max[i],
                                             tree);
        if(isnan(lnL)) break;
      }
    

  if(isnan(lnL) || isinf(FABS(lnL)))
    {
      tree->rates->c_lnL_times = UNLIKELY;
      return UNLIKELY;
    }

  tree->rates->c_lnL_times = lnL;
  return(lnL);
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

phydbl TIMES_Lk_Birth_Death_One_Node(phydbl t, phydbl min, phydbl max, t_tree *tree)
{
  phydbl lnL;
  phydbl p0t1,p0t,p1t,gt,vt1;
  phydbl b,d;
  phydbl t1;

  if(t > max || t < min) return NAN;

  t = FABS(t);

  lnL = 0.0;
  b   = tree->rates->birth_rate;
  d   = tree->rates->death_rate;
  t1  = FABS(tree->rates->nd_t[tree->n_root->num]);


  if(Are_Equal(b,d,1.E-6) == NO)
    {
      p0t1 = (b-d)/(b-d*EXP((d-b)*t1));
      vt1  = 1. - p0t1*EXP((d-b)*t1);
      
      p0t = (b-d)/(b-d*EXP((d-b)*t));
      p1t = p0t*p0t*EXP((d-b)*t);
      gt  = b*p1t/vt1;
      lnL = LOG(gt);
      return lnL;
    }
  else
    {
      // Equation 8 in Yang and Rannala (2006)
      // Node age outside its calibration boundaries
      
      gt  = (1.+b*t1)/(t1*(1.+b*t)*(1.+b*t));
      lnL = LOG(gt);
      return lnL;        
    }
  return NAN;
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

// Generate a subtree including all taxa in tax_list. The age of the root of that
// subtree is t_mrca. All nodes in the subtree are thus younger than that.
void TIMES_Connect_List_Of_Taxa(t_node **tax_list, int list_size, phydbl t_mrca, phydbl *times, int *nd_num, t_tree *mixt_tree)
{
  phydbl t_upper_bound, t_lower_bound;
  int i,j,n_anc,*permut;
  t_node *n,**anc,*new_mrca;

  t_lower_bound = t_mrca;
  t_upper_bound = 0.0;
  n             = NULL;
  anc           = NULL;
  new_mrca      = NULL;
  permut        = NULL;
  
  
  // Find the upper bound for all the new node ages that 
  // will be created in this function
  For(i,list_size)
    {
      n = tax_list[i];
      while(n->v[0] != NULL) n = n->v[0];
      if(times[n->num] < t_upper_bound) t_upper_bound = times[n->num];
    }
  
  /* printf("\n. upper: %f lower: %f t_mrca: %f\n",t_upper_bound,t_lower_bound,t_mrca); */
  assert(t_upper_bound > t_lower_bound);
  
  // Get the list of current mrcas to all taxa in tax_list. There should be
  // at least one of these
  n_anc = 0;
  For(i,list_size)
    {
      n = tax_list[i];
      while(n->v[0] != NULL) n = n->v[0];
      For(j,n_anc) if(anc[j] == n) break;
      if(j == n_anc)
        {
          if(n_anc == 0) anc = (t_node **)mCalloc(1,sizeof(t_node *));
          else           anc = (t_node **)mRealloc(anc,n_anc+1,sizeof(t_node *));
          anc[n_anc] = n;
          n_anc++;
        }
    }

  /* printf("\n. n_anc: %d",n_anc); */
  /* For(i,n_anc) PhyML_Printf("\n. anc: %d",anc[i]->num); */
  
  if(n_anc == 1) // All the nodes in tax_list are already connected. Bail out.
    {
      Free(anc);
      return;
    }
  
  // Connect randomly and set ages
  permut = Permutate(n_anc);
  i = 0;
  do
    {
      new_mrca               = mixt_tree->a_nodes[*nd_num];
      anc[permut[i]]->v[0]   = new_mrca;
      anc[permut[i+1]]->v[0] = new_mrca;
      new_mrca->v[1]         = anc[permut[i]];
      new_mrca->v[2]         = anc[permut[i+1]];
      new_mrca->v[0]         = NULL;
      times[new_mrca->num]   = Uni()*(t_upper_bound - t_lower_bound) + t_lower_bound;


      printf("\n. new_mrca->num: %d time: %f [%f %f] t_mrca: %f %d connect to %d %d %d [%f %f]",
             new_mrca->num,
             times[new_mrca->num],
             t_lower_bound,
             t_upper_bound,
             t_mrca,
             new_mrca->num,
             new_mrca->v[0] ? new_mrca->v[0]->num : -1,
             new_mrca->v[1] ? new_mrca->v[1]->num : -1,
             new_mrca->v[2] ? new_mrca->v[2]->num : -1,
             times[new_mrca->v[1]->num],
             times[new_mrca->v[2]->num]
             );
      fflush(NULL);

      t_upper_bound          = times[new_mrca->num]; // Might give a funny distribution of node ages, but should work nonetheless...
      anc[permut[i+1]]       = new_mrca;
      n_anc--;
      i++;
      (*nd_num) += 1;
      if(n_anc == 1) times[new_mrca->num] = t_mrca;
    }
  while(n_anc != 1);
  
  Free(permut);
  Free(anc);
}
  
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// Generate a  random rooted tree with node ages fullfiling the
// time constraints defined in the list of calibration cal_list 
void TIMES_Randomize_Tree_With_Time_Constraints(t_cal *cal_list, t_tree *mixt_tree)
{
  t_node **tips,**nd_list;
  phydbl *times,*cal_times,time_oldest_cal;
  int i,j,k,nd_num,*cal_ordering,n_cal,swap,list_size,tmp,orig_is_mixt_tree,repeat,n_max_repeats;
  t_cal *cal;
  
  assert(mixt_tree->rates);
  
  tips    = (t_node **)mCalloc(mixt_tree->n_otu,sizeof(t_node *));
  nd_list = (t_node **)mCalloc(mixt_tree->n_otu,sizeof(t_node *));
  
  times                   = mixt_tree->rates->nd_t;
  orig_is_mixt_tree       = mixt_tree->is_mixt_tree;
  mixt_tree->is_mixt_tree = NO;
  n_max_repeats           = 1000;

  For(repeat,n_max_repeats)
    {
      nd_num = mixt_tree->n_otu;
  
      /* PhyML_Printf("\n\n. Repeat %d",repeat); */

      For(i,mixt_tree->n_otu) tips[i] = mixt_tree->a_nodes[i];
      For(i,mixt_tree->n_otu) mixt_tree->a_nodes[i]->v[0] = NULL; 


      // Set a time for each calibration 
      cal_times = (phydbl *)mCalloc(1,sizeof(phydbl));
      time_oldest_cal = 0.0;
      n_cal = 0;
      cal = cal_list;
      while(cal != NULL)
        {
          if(cal->is_primary == YES)
            {
              if(n_cal > 0) cal_times = (phydbl *)mRealloc(cal_times,n_cal+1,sizeof(phydbl));
              
              cal_times[n_cal] = Uni()*(cal->upper - cal->lower) + cal->lower;
                            
              if(cal_times[n_cal] < time_oldest_cal) time_oldest_cal = cal_times[n_cal];
              
              n_cal++;
            }
          cal = cal->next;
        }
 
      /* printf("\n. n_cal : %d",n_cal); */
      /* Exit("\n"); */
     
      cal_ordering = (int *)mCalloc(n_cal,sizeof(int));
      For(i,n_cal) cal_ordering[i] = i;
      
      // Find the ordering of calibration times from youngest to oldest
      do
        {
          swap = NO;
          For(i,n_cal-1) 
            {
              if(cal_times[cal_ordering[i]] < cal_times[cal_ordering[i+1]])
                {
                  tmp               = cal_ordering[i+1];
                  cal_ordering[i+1] = cal_ordering[i];
                  cal_ordering[i]   = tmp;
                  swap = YES;
                }
            }
        }
      while(swap == YES);
      
      For(i,n_cal-1) assert(cal_times[cal_ordering[i]] > cal_times[cal_ordering[i+1]]);
      
        
      // Connect all taxa that appear in all primary calibrations
      For(i,n_cal)
        {
          cal = cal_list;
          j = 0;
          while(j!=cal_ordering[i]) 
            { 
              if(cal->is_primary == YES) j++; 
              cal = cal->next; 
              assert(cal); 
            }
          
          list_size = 0;
          For(j,cal->n_target_tax) 
            {
              For(k,mixt_tree->n_otu)  
                {
                  if(!strcmp(cal->target_tax[j],tips[k]->name))
                    {
                      nd_list[list_size] = tips[k];
                      list_size++;
                      break;
                    }
                }
            }
          
          assert(list_size == cal->n_target_tax);
          
          /* For(j,n_cal) */
          /*   { */
          /*     printf("\n. %d -> %f", */
          /*            cal_ordering[j], */
          /*            cal_times[cal_ordering[j]]); */
          /*     fflush(NULL); */
          /*   } */

          /* For(j,cal->n_target_tax) PhyML_Printf("\n. %s [%d]",nd_list[j]->name,nd_list[j]->num); */
          /* PhyML_Printf("\n. Time: %f [%f %f]", */
          /*              cal_times[cal_ordering[i]], */
          /*              cal->lower, */
          /*              cal->upper); */
          
          /* For(k,list_size) printf("\n@ %s",nd_list[k]->name); */
          /* printf("\n"); */

          printf("\n >>>>>>>>>>>>>>>>>>>>>>>>>>>>>> ");
          TIMES_Connect_List_Of_Taxa(nd_list, 
                                     cal->n_target_tax, 
                                     cal_times[cal_ordering[i]], 
                                     times, 
                                     &nd_num,
                                     mixt_tree);

        }
      
      Free(cal_times);
      Free(cal_ordering);
      
      
      // Connect all remaining taxa
      For(i,mixt_tree->n_otu) nd_list[i] = NULL;
      list_size = 0;
      For(i,mixt_tree->n_otu) 
        {
          if(tips[i]->v[0] == NULL) // Tip is not connected yet
            {
              nd_list[list_size] = tips[i];
              list_size++;
            }
        }

      cal = cal_list;
      do
        {
          For(k,mixt_tree->n_otu)  
            {
              if(!strcmp(cal->target_tax[0],tips[k]->name))
                {
                  nd_list[list_size] = tips[k];
                  list_size++;
                  break;
                }
            }
          cal = cal->next;
        }
      while(cal);

      /* For(i,list_size) printf("\n# To connect: %d",nd_list[i]->num); */
      
      /* printf("\n >>>>>>>>>>>>>>>>>>>>>>>>>>>>>> "); fflush(NULL); */
      TIMES_Connect_List_Of_Taxa(nd_list, 
                                 list_size,
                                 Uni()*time_oldest_cal + time_oldest_cal, 
                                 times, 
                                 &nd_num,
                                 mixt_tree);
      
      /* For(i,2*mixt_tree->n_otu-2) printf("\n. i:%d -> %d %d %d", */
      /*                                    i, */
      /*                                    mixt_tree->a_nodes[i]->v[0] ? mixt_tree->a_nodes[i]->v[0]->num : -1, */
      /*                                    mixt_tree->a_nodes[i]->v[1] ? mixt_tree->a_nodes[i]->v[1]->num : -1, */
      /*                                    mixt_tree->a_nodes[i]->v[2] ? mixt_tree->a_nodes[i]->v[2]->num : -1); */
            

      // Adding root node 
      mixt_tree->n_root = mixt_tree->a_nodes[2*mixt_tree->n_otu-2];
      mixt_tree->n_root->v[1]->v[0] = mixt_tree->n_root->v[2];
      mixt_tree->n_root->v[2]->v[0] = mixt_tree->n_root->v[1];
      Update_Ancestors(mixt_tree->n_root,mixt_tree->n_root->v[2],mixt_tree);
      Update_Ancestors(mixt_tree->n_root,mixt_tree->n_root->v[1],mixt_tree);
      mixt_tree->n_root->anc = NULL;

      // Adding root edge
      mixt_tree->num_curr_branch_available = 0;
      Connect_Edges_To_Nodes_Recur(mixt_tree->a_nodes[0],mixt_tree->a_nodes[0]->v[0],mixt_tree);
      Fill_Dir_Table(mixt_tree);
      Update_Dirs(mixt_tree);
      
      For(i,2*mixt_tree->n_otu-3)
        {
          if(((mixt_tree->a_edges[i]->left == mixt_tree->n_root->v[1]) || (mixt_tree->a_edges[i]->rght == mixt_tree->n_root->v[1])) &&
             ((mixt_tree->a_edges[i]->left == mixt_tree->n_root->v[2]) || (mixt_tree->a_edges[i]->rght == mixt_tree->n_root->v[2])))
            {
              Add_Root(mixt_tree->a_edges[i],mixt_tree);
              break;
            }
        }
      
      DATE_Assign_Primary_Calibration(mixt_tree);
      DATE_Update_T_Prior_MinMax(mixt_tree);

      /* { */
      /*   Print_Node(mixt_tree->n_root,mixt_tree->n_root->v[1],mixt_tree); */
      /*   Print_Node(mixt_tree->n_root,mixt_tree->n_root->v[2],mixt_tree); */
      /*   fflush(NULL); */

      /*   int i; */
      /*   For(i,mixt_tree->rates->n_cal) */
      /*     { */
      /*       PhyML_Printf("\n. Node number to which calibration [%d] applies to is [%d]",i,Find_Clade(mixt_tree->rates->a_cal[i]->target_tax, */
      /*                                                                                                mixt_tree->rates->a_cal[i]->n_target_tax, */
      /*                                                                                                mixt_tree)); */
      /*       PhyML_Printf("\n. Lower bound set to: %15f time units.",mixt_tree->rates->a_cal[i]->lower); */
      /*       PhyML_Printf("\n. Upper bound set to: %15f time units.",mixt_tree->rates->a_cal[i]->upper); */
      /*     } */
      /* } */

      if(!DATE_Check_Calibration_Constraints(mixt_tree) ||
         !DATE_Check_Time_Constraints(mixt_tree))
        {
          PhyML_Printf("\n. Could not generate tree");
        }
      else break; // Tree successfully generated
    }

  if(repeat == n_max_repeats)
    {
      PhyML_Printf("\n\n");
      PhyML_Printf("\n== A random tree satisfying the calibration constraints provided");
      PhyML_Printf("\n== could not be generated. It probably means that there are some");
      PhyML_Printf("\n== inconsistencies in the calibration data. For instance, the calibration");
      PhyML_Printf("\n== time interval for the MRCA of a clade with taxa {X,Y} (noted as [a,b])");
      PhyML_Printf("\n== cannot be strictly older than the interval corresponding to taxa ");
      PhyML_Printf("\n== {X,Z,Y} (noted as [c,d]), i.e., b cannot be smaller (older) than c. ");
      PhyML_Printf("\n== Also, please remember that the present time corresponds to a time");
      PhyML_Printf("\n== value equal to zero and past events have negative time values.");
      Exit("\n");
    }

  /* For(j,mixt_tree->n_otu) printf("\n. %s",mixt_tree->a_nodes[j]->name); */
        
  assert(i != 2*mixt_tree->n_otu-3);

  mixt_tree->is_mixt_tree = orig_is_mixt_tree;

  if(mixt_tree->is_mixt_tree == YES)
    {
      t_tree *tree;
      
      // Propagate tree topology and reorganize partial lk struct along edges
      tree = mixt_tree->next;
      do
        {
          For(i,2*tree->n_otu-1)
            {
              tree->a_nodes[i]->v[0] = tree->prev->a_nodes[i]->v[0] ? tree->a_nodes[tree->prev->a_nodes[i]->v[0]->num] : NULL;
              tree->a_nodes[i]->v[1] = tree->prev->a_nodes[i]->v[1] ? tree->a_nodes[tree->prev->a_nodes[i]->v[1]->num] : NULL;
              tree->a_nodes[i]->v[2] = tree->prev->a_nodes[i]->v[2] ? tree->a_nodes[tree->prev->a_nodes[i]->v[2]->num] : NULL;
            }
          tree->num_curr_branch_available = 0;
          Connect_Edges_To_Nodes_Recur(tree->a_nodes[0],tree->a_nodes[0]->v[0],tree);
          Fill_Dir_Table(tree);
          Update_Dirs(tree);
          Add_Root(tree->a_edges[tree->prev->e_root->num],tree);
          Reorganize_Edges_Given_Lk_Struct(tree);
          
          /* { */
          /*   printf("\n === \n"); */
          /*   Print_Node(tree->n_root,tree->n_root->v[1],tree); */
          /*   Print_Node(tree->n_root,tree->n_root->v[2],tree); */
          /* } */
          
          tree = tree->next;
        }
      while(tree);
    }
  
  /* printf("\n >>> \n"); */
  /* Print_Node(mixt_tree->n_root,mixt_tree->n_root->v[1],mixt_tree); */
  /* Print_Node(mixt_tree->n_root,mixt_tree->n_root->v[2],mixt_tree); */

  /* PhyML_Printf("\n. %s \n",Write_Tree(mixt_tree,NO)); */

  Free(tips);
  Free(nd_list);
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

int TIMES_Check_Node_Height_Ordering(t_tree *tree)
{
  if(!TIMES_Check_Node_Height_Ordering_Post(tree->n_root,tree->n_root->v[1],tree)) return NO;
  if(!TIMES_Check_Node_Height_Ordering_Post(tree->n_root,tree->n_root->v[2],tree)) return NO;
  return YES;
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

int TIMES_Check_Node_Height_Ordering_Post(t_node *a, t_node *d, t_tree *tree)
{

  if(tree->rates->nd_t[d->num] < tree->rates->nd_t[a->num])
    {
      PhyML_Printf("\n== a->t = %f[%d] d->t %f[%d]",
                   tree->rates->nd_t[a->num],
                   a->num,
                   tree->rates->nd_t[d->num],
                   d->num);
      return NO;
    }
  if(d->tax == YES) return YES;
  else
    {
      int i;
      For(i,3)
        {
          if(d->v[i] != a && d->b[i] != tree->e_root)
            if(!TIMES_Check_Node_Height_Ordering_Post(d,d->v[i],tree))
              return NO;
        }
    }
  return YES;
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
