#ifndef LINEAR_SOLVERS_MUMPS_SPARSE_H
#define LINEAR_SOLVERS_MUMPS_SPARSE_H

#include "SpLinearSolver.h"
#include <mpi.h>
#include "dmumps_c.h"

namespace LinearSolvers {

	class MumpsSparse: public SpLinearSolver {
		private:
		static constexpr int _use_comm_world = -987654;
		static constexpr int _job_init = -1;
		static constexpr int _job_end = -2;
		static constexpr int _job_analyze = 1;
		static constexpr int _job_factorize = 2;
		static constexpr int _job_solve = 3;
		static constexpr int _job_analyze_factorize = 4;
		static constexpr int _job_factorize_solve = 5;
		static constexpr int _job_all = 6;
		MPI_Comm _children;
		bool _children_is_empty;
		bool _parallel_flag;
		int _nproc;
		int err[4];
		DMUMPS_STRUC_C _id;
		public:
		MumpsSparse();
		MumpsSparse(const ParameterList &list);
		~MumpsSparse();
		virtual void factorize(const Eigen::SparseMatrix<double> &);
		virtual void solve(const Eigen::MatrixXd &);
		virtual void setParameters(const ParameterList &list);
		private:
		void setDefault();
	};

}

#endif
