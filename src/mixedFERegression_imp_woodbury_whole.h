#ifndef __MIXEDFEREGRESSION_IMP_HPP__
#define __MIXEDFEREGRESSION_IMP_HPP__

#include <iostream>
#include <random>
#include "timing.h"
#include <fstream>
#include <chrono>

#include "mpi.h"
#include "dmumps_c.h"
#define JOB_INIT -1
#define JOB_END -2
#define USE_COMM_WORLD -987654

////build system matrix in sparse format SWest non serve??
//void MixedFE::build(SpMat & L, SpMat& opMat, SpMat& opMat2, SpMat& mass, const VectorXr& righthand, const VectorXr& forcing_term )
//{
//	if(opMat2.rows()!=mass.rows() || L.rows()!=opMat.rows() )
//	 std::cerr<<"incompatible blocks, dimension mismatch"<<std::endl;
//
//	if(righthand.size()+forcing_term.size()!=2*opMat.rows() )
//	 std::cerr<<"incompatible right hand side forcing term, dimension mismatch"<<std::endl;
//
//
//	UInt nnodes=opMat.rows();
//
//
//	/*SpMat tempStiff(_femhandl.getStiff()),
//			tempMass(_femhandl.getMass());*/
//
//	//I reserve the exact memory for the nonzero entries of each row of the coeffmatrix for boosting performance
//	_coeffmatrix.resize(2*nnodes,2*nnodes);
//	std::vector<int> entries(2*nnodes);
//	UInt number=0,number2=0;
//
//	for(auto k=0; k<nnodes; k++)
//	{
//
//	 number=L.col(k).nonZeros()+opMat.col(k).nonZeros();
//
//	 entries[k]=number;
//
//	 number2=opMat2.col(k).nonZeros()+mass.col(k).nonZeros();
//
//	 entries[nnodes+k]=number2;
//	}
//
//	_coeffmatrix.reserve(entries);
//	////building system matrix
//	for(auto i=0; i<nnodes; i++)
//	{
//		// north-west block from matrix L, cycling over non-zero elements
//		for(SpMat::InnerIterator it(L,i); it; ++it)
//		{
//			_coeffmatrix.insert(it.index(),i)=it.value();
//		}
//		// north-east cycling over non-zero elements
//		for(SpMat::InnerIterator it(opMat,i); it; ++it)
//		{
//			_coeffmatrix.insert(it.index(),i+nnodes)=it.value(); //north-east block
//			//_coeffmatrix.insert(it.index()+nnodes,i)=it.value(); //south-west block
//		}
//		// south-west block cycling over non-zero elements
//		for(SpMat::InnerIterator it(opMat2,i); it; ++it)
//		{
//		    _coeffmatrix.insert(it.index()+nnodes,i)=it.value(); //south-west block
//	    }
//
//		//south-east block from Mass matrix, cycling over non-zero elements
//		for(SpMat::InnerIterator it(mass,i); it; ++it)
//		{
//			_coeffmatrix.insert(it.index()+nnodes,i+nnodes)=it.value();
//		}
//	}
//	_coeffmatrix.makeCompressed();
//
//	_b.resize(nnodes*2);
//
//	_b.topRows(righthand.rows())=righthand;
//
//	_b.bottomRows(forcing_term.rows())=forcing_term;
//
//}

template<typename InputHandler, typename Integrator, UInt ORDER>
void MixedFERegression<InputHandler,Integrator,ORDER>::buildCoeffMatrix(const SpMat& DMat,  const SpMat& AMat,  const SpMat& MMat)
{
	//I reserve the exact memory for the nonzero entries of each row of the coeffmatrix for boosting performance
	//_coeffmatrix.setFromTriplets(tripletA.begin(),tripletA.end());

	UInt nnodes = mesh_.num_nodes();

		std::vector<coeff> tripletAll;
	tripletAll.reserve(DMat.nonZeros() + 2*AMat.nonZeros() + MMat.nonZeros());

	for (int k=0; k<DMat.outerSize(); ++k)
	  for (SpMat::InnerIterator it(DMat,k); it; ++it)
	  {
		  tripletAll.push_back(coeff(it.row(), it.col(),it.value()));
	  }
	for (int k=0; k<MMat.outerSize(); ++k)
	  for (SpMat::InnerIterator it(MMat,k); it; ++it)
	  {
		  tripletAll.push_back(coeff(it.row()+nnodes, it.col()+nnodes,it.value()));
	  }
	for (int k=0; k<AMat.outerSize(); ++k)
	  for (SpMat::InnerIterator it(AMat,k); it; ++it)
	  {
		  tripletAll.push_back(coeff(it.col(), it.row()+nnodes,it.value()));
	  }
	for (int k=0; k<AMat.outerSize(); ++k)
	  for (SpMat::InnerIterator it(AMat,k); it; ++it)
	  {
		  tripletAll.push_back(coeff(it.row()+nnodes, it.col(), it.value()));
	  }

	_coeffmatrix.setZero();
	_coeffmatrix.resize(2*nnodes,2*nnodes);
	_coeffmatrix.setFromTriplets(tripletAll.begin(),tripletAll.end());
	_coeffmatrix.makeCompressed();
	//std::cout<<"Coefficients' Matrix Set Correctly"<<std::endl;
}

template<typename InputHandler, typename Integrator, UInt ORDER>
void MixedFERegression<InputHandler,Integrator,ORDER>::addDirichletBC(const vector<int>& bcindex, const vector<Real>& bcvalues)
{


	UInt id1,id3;

	UInt nnodes = mesh_.num_nodes();

	const std::vector<UInt>& bc_indices = regressionData_.getDirichletIndices();
	const std::vector<Real>& bc_values = regressionData_.getDirichletValues();
	UInt nbc_indices = bc_indices.size();

	Real pen=10e20;

	for( auto i=0; i<nbc_indices; i++)
	 {
			id1=bcindex[i];
			id3=id1+nnodes;

			//_coeffmatrix.prune([id1,id3](int i, int j, Real) { return (i!=id1 && i!=id3); });

			_coeffmatrix.coeffRef(id1,id1)=pen;
			_coeffmatrix.coeffRef(id3,id3)=pen;


			_b(id1)+=bc_values[i]*pen;
			_b(id3)=0;
	 }

	_coeffmatrix.makeCompressed();
}

//construct NW block of the system matrix in Ramsay with covariates format
//!! Depends on setPsi and setQ
template<typename InputHandler, typename Integrator, UInt ORDER>
void MixedFERegression<InputHandler,Integrator,ORDER>::getDataMatrix(SpMat& DMat)
{
		UInt nnodes = mesh_.num_nodes();
		//UInt nlocations = regressionData_.getNumberofObservations();

		DMat.resize(nnodes,nnodes);

		if (regressionData_.getCovariates().rows() == 0)
			DMat = psi_.transpose()*psi_;
		else
		{
			DMat = (SpMat(psi_.transpose())*Q_*psi_).sparseView();
		}
}

template<typename InputHandler, typename Integrator, UInt ORDER>
void MixedFERegression<InputHandler,Integrator,ORDER>::getDataMatrixByIndices(SpMat& DMat)
{
		UInt nnodes = mesh_.num_nodes();
		UInt nlocations = regressionData_.getNumberofObservations();

		DMat.resize(nnodes,nnodes);

		if (regressionData_.getCovariates().rows() == 0)
		{
			DMat.reserve(1);
			for (auto i = 0; i<nlocations; ++i)
			{
				auto index = regressionData_.getObservationsIndices()[i];
				DMat.insert(index,index) = 1;
			}
		}
		else
		{
			//May be inefficient
			for (auto i = 0; i<nlocations; ++i)
			{
				auto index_i = regressionData_.getObservationsIndices()[i];
				for (auto j = 0; j<nlocations; ++j)
				{
					auto index_j = regressionData_.getObservationsIndices()[j];
					DMat.insert(index_i,index_j) = Q_(i,j);
				}
			}
		}
}

template<typename InputHandler, typename Integrator, UInt ORDER>
void MixedFERegression<InputHandler,Integrator,ORDER>::setPsi(){

	//std::cout<<"Data Matrix Computation by Basis Evaluation.."<<std::endl;
	UInt nnodes = mesh_.num_nodes();
	UInt nlocations = regressionData_.getNumberofObservations();

	//cout<<"Nodes number "<<nnodes<<"Locations number "<<nlocations<<endl;

	//std::vector<coeff> entries;
	//entries.resize((ORDER * 3)*nlocations);


	psi_.resize(nlocations, nnodes);
	//psi_.reserve(Eigen::VectorXi::Constant(nlocations,ORDER*3));
	if (regressionData_.isLocationsByNodes()){
		std::vector<coeff> tripletAll;
		auto k = regressionData_.getObservationsIndices();
		tripletAll.reserve(k.size());
		for (int i = 0; i< k.size(); ++i){
			tripletAll.push_back(coeff(i,k[i],1.0));
		}
    	psi_.setFromTriplets(tripletAll.begin(),tripletAll.end());
    	psi_.makeCompressed();
    }
    else {
		Triangle<ORDER*3> tri_activated;
		Eigen::Matrix<Real,ORDER * 3,1> coefficients;

		Real evaluator;

		for(UInt i=0; i<nlocations;i++)
		{
			tri_activated = mesh_.findLocationNaive(regressionData_.getLocations()[i]);
			if(tri_activated.getId() == Identifier::NVAL)
			{
				#ifdef R_VERSION_
				Rprintf("ERROR: Point %d is not in the domain, remove point and re-perform smoothing\n", i+1);
				#else
				std::cout << "ERROR: Point " << i+1 <<" is not in the domain\n";
				#endif
			}else
			{
				for(UInt node = 0; node < ORDER*3 ; ++node)
				{
					coefficients = Eigen::Matrix<Real,ORDER * 3,1>::Zero();
					coefficients(node) = 1; //Activates only current base
					evaluator = evaluate_point<ORDER>(tri_activated, regressionData_.getLocations()[i], coefficients);
					psi_.insert(i, tri_activated[node].getId()) = evaluator;
				}
			}
		}

		psi_.makeCompressed();
	}
}

template<typename InputHandler, typename Integrator, UInt ORDER>
void MixedFERegression<InputHandler,Integrator,ORDER>::setQ()
{
	//std::cout<<"Computing Orthogonal Space Projection Matrix"<<std::endl;
	Q_.resize(H_.rows(),H_.cols());
	Q_ = -H_;
	for (int i=0; i<H_.rows();++i)
	{
		Q_(i,i) += 1;
	}
}

template<typename InputHandler, typename Integrator, UInt ORDER>
void MixedFERegression<InputHandler,Integrator,ORDER>::setH()
{
	//std::cout<<"Computing Projection Matrix"<<std::endl;
	//UInt nnodes = mesh_.num_nodes();
	UInt nlocations = regressionData_.getNumberofObservations();

	//regressionData_.printCovariates(std::cout);
	MatrixXr W(this->regressionData_.getCovariates());
	//std::cout<<"W "<< W <<std::endl;
	//total number of mesh nodes
	//UInt nnodes = mesh_.num_nodes();
	if(regressionData_.isLocationsByNodes())
	{
		MatrixXr W_reduced(regressionData_.getNumberofObservations(), W.cols());
		for (auto i=0; i<nlocations;++i)
		{
			auto index_i = regressionData_.getObservationsIndices()[i];
			for (auto j=0; j<W.cols();++j)
			{
				W_reduced(i,j) = W(index_i,j);
			}
		}
		W = W_reduced;
	}


	MatrixXr WTW(W.transpose()*W);

	H_=W*WTW.ldlt().solve(W.transpose()); // using cholesky LDLT decomposition for computing hat matrix
}

template<typename InputHandler, typename Integrator, UInt ORDER>
void MixedFERegression<InputHandler,Integrator,ORDER>::getRightHandData(VectorXr& rightHandData)
{
	UInt nnodes = mesh_.num_nodes();
	UInt nlocations = regressionData_.getNumberofObservations();
	//rightHandData.resize(nnodes);
	rightHandData = VectorXr::Zero(nnodes);

	if(regressionData_.getCovariates().rows() == 0)
	{
		if(regressionData_.isLocationsByNodes())
		{

			for (auto i=0; i<nlocations;++i)
			{
				auto index_i = regressionData_.getObservationsIndices()[i];
				rightHandData(index_i) = regressionData_.getObservations()[i];
			}
		}
		else
		{
			rightHandData=psi_.transpose()*regressionData_.getObservations();
		}
	}
	else
	{
		rightHandData=psi_.transpose()*LeftMultiplybyQ(regressionData_.getObservations());
	}
}

template<typename InputHandler, typename Integrator, UInt ORDER>
void MixedFERegression<InputHandler,Integrator,ORDER>::computeDegreesOfFreedom(UInt output_index, Real lambda)
{
	int GCVmethod = regressionData_.getGCVmethod();
	switch (GCVmethod) {
		case 1:
			computeDegreesOfFreedomExact(output_index, lambda);
			break;
		case 2:
			computeDegreesOfFreedomStochastic(output_index, lambda);
			break;
	}
}

template<typename InputHandler, typename Integrator, UInt ORDER>
void MixedFERegression<InputHandler,Integrator,ORDER>::computeDegreesOfFreedomExact(UInt output_index, Real lambda)
{
   	timer clock;
	clock.start();
    UInt nnodes = mesh_.num_nodes();
	UInt nlocations = regressionData_.getNumberofObservations();
    Real degrees=0;

    //if caso bello bello
    if (regressionData_.isLocationsByNodes() && regressionData_.getCovariates().rows() == 0 )
    {
    	SpMat DMat = psi_.transpose()*psi_;
    	SpMat AMat_lambda = (-lambda)*AMat_;
    	SpMat MMat_lambda = (-lambda)*MMat_;
    	buildCoeffMatrix(DMat, AMat_lambda, MMat_lambda);
        auto k = regressionData_.getObservationsIndices();
        DMUMPS_STRUC_C id;
        int myid, ierr;
        ierr = MPI_Comm_rank(MPI_COMM_WORLD, &myid);

        id.sym=0;
        id.par=1;
        id.job=JOB_INIT;
        id.comm_fortran=USE_COMM_WORLD;
        dmumps_c(&id);

        std::vector<int> irn;
        std::vector<int> jcn;
        std::vector<double> a;
        std::vector<int> irhs_ptr;
        std::vector<int> irhs_sparse;
        double* rhs_sparse= (double*)malloc(nlocations*sizeof(double));
        
        if( myid==0){
            id.n=2*nnodes;
            for (int j=0; j<_coeffmatrix.outerSize(); ++j){
                for (SpMat::InnerIterator it(_coeffmatrix,j); it; ++it){
                	
                    irn.push_back(it.row()+1);
                    jcn.push_back(it.col()+1);
                    a.push_back(it.value());
                }
            }
        }
        id.nz=irn.size();
        id.irn=irn.data();
        id.jcn=jcn.data();
        id.a=a.data();
        id.nz_rhs=nlocations;
        id.nrhs=2*nnodes;
        int j = 1;
        irhs_ptr.push_back(j);
        for (int l=0; l<k[0]-1; ++l) {
            irhs_ptr.push_back(j);
        }
        for (int i=0; i<k.size()-1; ++i) {
            ++j;
            for (int l=0; l<k[i+1]-k[i]; ++l) {
                irhs_ptr.push_back(j);
            }
        }
        ++j;
        for (int i=k[k.size()-1]; i < id.nrhs; ++i) {
            irhs_ptr.push_back(j);
        }
        for (int i=0; i<nlocations; ++i){
            irhs_sparse.push_back(k[i]+1);
        }
        id.irhs_sparse=irhs_sparse.data();
        id.irhs_ptr=irhs_ptr.data();
        id.rhs_sparse=rhs_sparse;

        #define ICNTL(I) icntl[(I)-1]
//        id.ICNTL(1)=-1;
//        id.ICNTL(2)=-1;
//        id.ICNTL(3)=-1;
//        id.ICNTL(4)=0;
        id.ICNTL(5)=0;
        id.ICNTL(18)=0;
        id.ICNTL(20)=1;
        id.ICNTL(30)=1;

        id.job=6;
        dmumps_c(&id);
        id.job=JOB_END;
        dmumps_c(&id);

        if (myid==0){
            std::cout << "rhs = " << std::endl;
            for (int i=0; i< nlocations; ++i){
                std::cout << rhs_sparse[i] << std::endl;
                degrees+=rhs_sparse[i];
            }
        }
        free(rhs_sparse);
    }
    else{ //non siamo nel caso 4 => montare i solver   
        MatrixXr DMat = psi_.transpose() * LeftMultiplybyQ(psi_);
        Eigen::SparseLU<SpMat> solver;
        solver.compute(MMat_);
        SpMat U = solver.solve(AMat_);
        MatrixXr Ud = MatrixXr(U);
        auto k = regressionData_.getObservationsIndices();
        SpMat tempSparse = lambda*AMat_.transpose()*U;
		MatrixXr temp = MatrixXr(tempSparse);
		
        //MatrixXr Td = MatrixXr(DMat + lambda*AMat_.transpose()*U);
        MatrixXr Td = DMat + temp;
        Eigen::LLT<MatrixXr> Dsolver(Td);

        //caso 3)
        if(regressionData_.isLocationsByNodes() && regressionData_.getCovariates().rows() != 0) {
            // Setup rhs B
            MatrixXr B;
            B = MatrixXr::Zero(nnodes,nlocations);
            degrees += regressionData_.getCovariates().cols();
            // B = I(:,k) * Q
            for (auto i=0; i<nlocations;++i) {
            	VectorXr ei = VectorXr::Zero(nlocations);
            	ei(i) = 1;
            	VectorXr Qi = LeftMultiplybyQ(ei);
                for (int j=0; j<nlocations; ++j) {
                    B(k[i], j) = Qi(j);
                }
            }
            // Solve the system TX = B
            MatrixXr X;
            X=Dsolver.solve(B);
            // Compute trace(X(k,:))
            for (int i = 0; i < k.size(); ++i) {
                degrees += X(k[i], i);
            }
        }

        //if casi 1) e 2)
        if (!regressionData_.isLocationsByNodes()){
            MatrixXr X;
            X = Dsolver.solve(MatrixXr(DMat));
            //solo in caso 1) con covariate
            if (regressionData_.getCovariates().rows() != 0) {
                degrees += regressionData_.getCovariates().cols();
            }
            for (int i = 0; i<nnodes; ++i) {
                degrees += X(i,i);
            }
        }
    }
    _dof[output_index] = degrees;
    _var[output_index] = 0;
    std::cout << "Time required for GCV computation" << std::endl;
    clock.stop();
}

template<typename InputHandler, typename Integrator, UInt ORDER>
void MixedFERegression<InputHandler,Integrator,ORDER>::computeDegreesOfFreedomStochastic(UInt output_index, Real lambda)
{
	timer clock1;
	clock1.start();
	UInt nnodes = mesh_.num_nodes();
	UInt nlocations = regressionData_.getNumberofObservations();

	std::default_random_engine generator;
	// Set the initial state of the random number generator
	if (regressionData_.getRNGstate() != "") {
		std::stringstream initialRNGstate;
		initialRNGstate << regressionData_.getRNGstate();
		initialRNGstate >> generator;
	}
	// Creation of the random matrix
	std::bernoulli_distribution distribution(0.5);
	UInt nrealizations = regressionData_.getNrealizations();
	MatrixXr u(nlocations, nrealizations);
	for (int j=0; j<nrealizations; ++j) {
		for (int i=0; i<nlocations; ++i) {
			if (distribution(generator)) {
				u(i,j) = 1.0;
			}
			else {
				u(i,j) = -1.0;
			}
		}
	}
	// We have finished using the random number generator: we can save its state
	std::stringstream finalRNGstate;
	finalRNGstate << generator;
	finalRNGstate >> _finalRNGstate;

	// Define the first right hand side : | I  0 |^T * psi^T * Q * u
	MatrixXr b = MatrixXr::Zero(2*nnodes,u.cols());
	b.topRows(nnodes) = psi_.transpose()* LeftMultiplybyQ(u);

	// Resolution of the system
	MatrixXr x = system_solve(b);

	MatrixXr uTpsi = u.transpose()*psi_;
	VectorXr edf_vect(nrealizations);
	Real q = 0;
	Real var = 0;

	// Degrees of freedom = q + E[ u^T * psi * | I  0 |* x ]
	if (regressionData_.getCovariates().rows() != 0) {
		q = regressionData_.getCovariates().cols();
	}
	// For any realization we calculate the degrees of freedom
	for (int i=0; i<nrealizations; ++i) {
		edf_vect(i) = uTpsi.row(i).dot(x.col(i).head(nnodes)) + q;
		var += edf_vect(i)*edf_vect(i);
	}

	// Estimates: sample mean, sample variance, sample standard variation
	Real mean = edf_vect.sum()/nrealizations;
	_dof[output_index] = mean;
	var /= nrealizations;
	var -= mean*mean;
	_var[output_index]=var;
	Real std = sqrt(var);
	std::cout << "edf mean = " << mean << std::endl;

	std::cout << "Time required for GCV computation" << std::endl;
	clock1.stop();
}



//Implementation kept from Sangalli et al
template<typename InputHandler, typename Integrator, UInt ORDER>
void MixedFERegression<InputHandler,Integrator,ORDER>::smoothLaplace()
{
	//std::cout<<"Laplace Penalization - Order: "<<ORDER<<std::endl;

	//UInt ndata=regressionData_.getObservations().size();	
	
	UInt nnodes=mesh_.num_nodes();

	FiniteElement<Integrator, ORDER> fe;

	typedef EOExpr<Mass> ETMass;
	typedef EOExpr<Stiff> ETStiff;

	Mass EMass;
	Stiff EStiff;

	ETMass mass(EMass);
	ETStiff stiff(EStiff);

	setPsi();


//	if(!regressionData_.getCovariates().rows() == 0)
//	{
//		setH();
//		setQ();
//	}

//    if(!regressionData_.isLocationsByNodes())
//    {
//    	getDataMatrix(DMat_);
//    }
//    else
//    {
//    	getDataMatrixByIndices(DMat_);
//    }
    //std::cout<<"Block Data"<<DMat_<<std::endl;


    Assembler::operKernel(stiff, mesh_, fe, AMat_);
    Assembler::operKernel(mass, mesh_, fe, MMat_);

    VectorXr rightHandData;
    getRightHandData(rightHandData);
    _b = VectorXr::Zero(2*nnodes);
    _b.topRows(nnodes)=rightHandData;
    //std::cout<<"b vector"<<_b;

    _solution.resize(regressionData_.getLambda().size());
    _dof.resize(regressionData_.getLambda().size());
    _var.resize(regressionData_.getLambda().size());

    for(UInt i = 0; i<regressionData_.getLambda().size(); ++i)
	{
    	//build(tripletsData_,(-regressionData_.getLambda())*stiff, (-regressionData_.getLambda())*mass, righthand, forcing);

    	Real lambda = regressionData_.getLambda()[i];
    	SpMat AMat_lambda = (-lambda)*AMat_;
    	SpMat MMat_lambda = (-lambda)*MMat_;
    	this->buildA(psi_, AMat_lambda, MMat_lambda);

    	//Appling border conditions if necessary
    	if(regressionData_.getDirichletIndices().size() != 0)
    		addDirichletBC(regressionData_.getDirichletIndices(), regressionData_.getDirichletValues());

    	//prova.solveSystem<SpConjGrad>();
    	system_factorize();
    	_solution[i] = this->template system_solve(this->_b);
    	if(regressionData_.computeDOF())
    		computeDegreesOfFreedom(i, lambda);
    	else
    		_dof[i] = -1;

	}

}

//Implementation kept from Sangalli et al
template<typename InputHandler, typename Integrator, UInt ORDER>
void MixedFERegression<InputHandler,Integrator,ORDER>::smoothEllipticPDE()
{
	//std::cout<<"Elliptic PDE Penalization - Order: "<<ORDER<<std::endl;

	//UInt ndata=regressionData_.getObservations().size();
	UInt nnodes=mesh_.num_nodes();

	FiniteElement<Integrator, ORDER> fe;

	typedef EOExpr<Mass> ETMass;
	typedef EOExpr<Stiff> ETStiff;
	typedef EOExpr<Grad> ETGrad;

	Mass EMass;
	Stiff EStiff;
	Grad EGrad;

	ETMass mass(EMass);
	ETStiff stiff(EStiff);
	ETGrad grad(EGrad);

	setPsi();

	if(!regressionData_.getCovariates().rows() == 0)
	{
		setH();
		setQ();
	}

    if(!regressionData_.isLocationsByNodes())
    {
    	getDataMatrix(DMat_);
    }
    else
    {
    	getDataMatrixByIndices(DMat_);
    }
    //std::cout<<"Block Data"<<DMat<<std::endl;



    const Real& c = regressionData_.getC();
    const Eigen::Matrix<Real,2,2>& K = regressionData_.getK();
    const Eigen::Matrix<Real,2,1>& beta = regressionData_.getBeta();
    Assembler::operKernel(c*mass+stiff[K]+dot(beta,grad), mesh_, fe, AMat_);
    Assembler::operKernel(mass, mesh_, fe, MMat_);

    VectorXr rightHandData;
    getRightHandData(rightHandData);
    _b = VectorXr::Zero(2*nnodes);
    _b.topRows(nnodes)=rightHandData;
    //std::cout<<"b vector"<<_b;

    _solution.resize(regressionData_.getLambda().size());
    _dof.resize(regressionData_.getLambda().size());

    for(UInt i = 0; i<regressionData_.getLambda().size(); ++i)
	{
    	//build(tripletsData_,(-regressionData_.getLambda())*stiff, (-regressionData_.getLambda())*mass, righthand, forcing);

    	Real lambda = regressionData_.getLambda()[i];
    	SpMat AMat_lambda = (-lambda)*AMat_;
    	SpMat MMat_lambda = (-lambda)*MMat_;
    	this->buildCoeffMatrix(DMat_, AMat_lambda, MMat_lambda);
    	this->buildA(psi_, AMat_lambda, MMat_lambda);

    	//std::cout<<"AMat"<<std::endl<<_coeffmatrix;


    	//Appling border conditions if necessary
    	if(regressionData_.getDirichletIndices().size() != 0)
    		addDirichletBC(regressionData_.getDirichletIndices(), regressionData_.getDirichletValues());

    	//prova.solveSystem<SpConjGrad>();
    	this-> template solve<SpLU>(i);
    	if(regressionData_.computeDOF())
    		computeDegreesOfFreedom(i, lambda);
    	else
    		_dof[i] = -1;

	}

}


//Implementation kept from Sangalli et al
template<typename InputHandler, typename Integrator, UInt ORDER>
void MixedFERegression<InputHandler,Integrator,ORDER>::smoothEllipticPDESpaceVarying()
{
	//std::cout<<"Space-varying Coefficient - Elliptic PDE Penalization - Order: "<<ORDER<<std::endl;
		//UInt ndata=regressionData_.getObservations().size();
		UInt nnodes=mesh_.num_nodes();

		FiniteElement<Integrator, ORDER> fe;

		typedef EOExpr<Mass> ETMass;
		typedef EOExpr<Stiff> ETStiff;
		typedef EOExpr<Grad> ETGrad;

		Mass EMass;
		Stiff EStiff;
		Grad EGrad;

		ETMass mass(EMass);
		ETStiff stiff(EStiff);
		ETGrad grad(EGrad);

	setPsi();

	if(!regressionData_.getCovariates().rows() == 0)
	{
		setH();
		setQ();
	}

	if(!regressionData_.isLocationsByNodes())
	{
		getDataMatrix(DMat_);
	}
	else
	{
		getDataMatrixByIndices(DMat_);
	}
	//std::cout<<"Block Data"<<DMat_<<std::endl;

	const Reaction& c = regressionData_.getC();
	const Diffusivity& K = regressionData_.getK();
	const Advection& beta = regressionData_.getBeta();
	Assembler::operKernel(c*mass+stiff[K]+dot(beta,grad), mesh_, fe, AMat_);
	Assembler::operKernel(mass, mesh_, fe, MMat_);

	const ForcingTerm& u = regressionData_.getU();
	//for(auto i=0;i<18;i++) std::cout<<u(i)<<std::endl;
	VectorXr forcingTerm;
	Assembler::forcingTerm(mesh_,fe, u, forcingTerm);

	VectorXr rightHandData;
	getRightHandData(rightHandData);
	//_b.resize(2*nnodes);
	_b = VectorXr::Zero(2*nnodes);
	_b.topRows(nnodes)=rightHandData;
	//std::cout<<"Forcing Term "<<std::cout<<forcingTerm<<"END";
	_b.bottomRows(nnodes)=forcingTerm;
	//std::cout<<"b vector"<<_b;

	_solution.resize(regressionData_.getLambda().size());
	_dof.resize(regressionData_.getLambda().size());

	for(UInt i = 0; i<regressionData_.getLambda().size(); ++i)
	{
		//build(tripletsData_,(-regressionData_.getLambda())*stiff, (-regressionData_.getLambda())*mass, righthand, forcing);

		Real lambda = regressionData_.getLambda()[i];
		SpMat AMat_lambda = (-lambda)*AMat_;
		SpMat MMat_lambda = (-lambda)*MMat_;
		this->buildCoeffMatrix(DMat_, AMat_lambda, MMat_lambda);
		this->buildA(psi_, AMat_lambda, MMat_lambda);

		//std::cout<<"AMat"<<std::endl<<_coeffmatrix;


		//Appling border conditions if necessary
		if(regressionData_.getDirichletIndices().size() != 0)
			addDirichletBC(regressionData_.getDirichletIndices(), regressionData_.getDirichletValues());

		//prova.solveSystem<SpConjGrad>();

		//std::cout<< _coeffmatrix;
		this-> template solve<SpLU>(i);
		if(regressionData_.computeDOF())
			computeDegreesOfFreedom(i, lambda);
		else
			_dof[i] = -1;

	}

}


//solve sparse system with P method

template<typename InputHandler, typename Integrator, UInt ORDER>
template <typename P>
void MixedFERegression<InputHandler,Integrator,ORDER>::solve(UInt output_index)
{
	//std::cout<<this->_coeffmatrix;
	this->_solution[output_index].resize(this->_coeffmatrix.rows());
	P::solve(this->_coeffmatrix,this->_b,this->_solution[output_index]);
}

template<typename InputHandler, typename Integrator, UInt ORDER>
MatrixXr MixedFERegression<InputHandler,Integrator,ORDER>::LeftMultiplybyQ(const MatrixXr& u)
{	
	if (regressionData_.getCovariates().rows() == 0){
		return u;
	}
	else{
		MatrixXr W(this->regressionData_.getCovariates());
		if (isWTWfactorized_ == false ){
			WTWinv_.compute(W.transpose()*W);
			isWTWfactorized_=true;
		}
		MatrixXr Pu= W*WTWinv_.solve(W.transpose()*u);
		return u-Pu;
	}

}

template<typename InputHandler, typename Integrator, UInt ORDER>
void MixedFERegression<InputHandler,Integrator,ORDER>::buildA(const SpMat& Psi,  const SpMat& AMat,  const SpMat& MMat) {
		//I reserve the exact memory for the nonzero entries of each row of the coeffmatrix for boosting performance
	//_coeffmatrix.setFromTriplets(tripletA.begin(),tripletA.end());

	UInt nnodes = mesh_.num_nodes();
	
	SpMat DMat = Psi.transpose()*Psi;

	std::vector<coeff> tripletAll;
	tripletAll.reserve(DMat.nonZeros() + 2*AMat.nonZeros() + MMat.nonZeros());

	for (int k=0; k<DMat.outerSize(); ++k)
	  for (SpMat::InnerIterator it(DMat,k); it; ++it)
	  {
		  tripletAll.push_back(coeff(it.row(), it.col(),it.value()));
	  }
	for (int k=0; k<MMat.outerSize(); ++k)
	  for (SpMat::InnerIterator it(MMat,k); it; ++it)
	  {
		  tripletAll.push_back(coeff(it.row()+nnodes, it.col()+nnodes,it.value()));
	  }
	for (int k=0; k<AMat.outerSize(); ++k)
	  for (SpMat::InnerIterator it(AMat,k); it; ++it)
	  {
		  tripletAll.push_back(coeff(it.col(), it.row()+nnodes,it.value()));
	  }
	for (int k=0; k<AMat.outerSize(); ++k)
	  for (SpMat::InnerIterator it(AMat,k); it; ++it)
	  {
		  tripletAll.push_back(coeff(it.row()+nnodes, it.col(), it.value()));
	  }

	A_.setZero();
	A_.resize(2*nnodes,2*nnodes);
	A_.setFromTriplets(tripletAll.begin(),tripletAll.end());
	A_.makeCompressed();
	//std::cout<<"Coefficients' Matrix Set Correctly"<<std::endl;
}

template<typename InputHandler, typename Integrator, UInt ORDER>
void MixedFERegression<InputHandler,Integrator,ORDER>::system_factorize() {

	UInt nnodes = mesh_.num_nodes();
	timer clock1;
	clock1.start();

	// First phase: Factorization of matrix A
	std::cout << "Factorization of A" << std::endl;
	
	// Definition of a list of parameters for the solver
	/*LinearSolvers::ParameterList list;
	list.set("icntl[14]",100);
	list.set("sym",2);
	list.set("par",1);
	list.set("nproc",2);
	Adec_->setParameters(list);
	std::cout << "Parameters set" << std::endl;*/

	// Invoke the factorization on matrix A
	Adec_->factorize(A_);
	
	// We access to this phase only if there are covariates, otherwise  G = 0 (?)
	if (regressionData_.getCovariates().rows() != 0) {
		// Second phase: factorization of matrix  G =  C + [V * A^-1 * U]
		std::cout << "Factorization of G" << std::endl;

		// Definition of matrix U = [ psi * W | 0 ]^T
		MatrixXr W(this->regressionData_.getCovariates());
		U_ = MatrixXr::Zero(2*nnodes, W.cols());
		U_.topRows(nnodes) = psi_.transpose()*W;
		Adec_->solve(U_);

		// D = U^T * A^-1 * U
		MatrixXr D = U_.transpose()*Adec_->getSolution();
		// G = C + D
		MatrixXr G = -W.transpose()*W + D;
		Gdec_.compute(G);
	}

	std::cout << "Time required to factorize the system" << std::endl;
	clock1.stop();
}

template<typename InputHandler, typename Integrator, UInt ORDER>
template<typename Derived>
MatrixXr MixedFERegression<InputHandler,Integrator,ORDER>::system_solve(const Eigen::MatrixBase<Derived> &b) {
	
	timer clock1;
	clock1.start();

	// Resolution of the system A * x1 = b
	std::cout << "Solving FEM: 1" << std::endl;
	Adec_->solve(b);
	MatrixXr x1 = Adec_->getSolution();
	
	// We access to this phase only if there are covariates, otherwise the solution is x1
	if (regressionData_.getCovariates().rows() != 0) {
		std::cout << "Solving FEM: 2" << std::endl;
		// Resolution of G * x2 = U^T * x1
		MatrixXr x2 = Gdec_.solve(U_.transpose()*x1);
		// Resolution of the system A * x3 = U * x2
		Adec_->solve(U_*x2);
		// We add the latest result to the x1 we computed previously
		x1 -= Adec_->getSolution();
	}

	std::cout << "Time required to solve the system" << std::endl;
	clock1.stop();
	return x1;
}

#endif
