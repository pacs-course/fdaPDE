checkSmoothingParameters<-function(
	locations = NULL,
	observations,
	FEMbasis,
	lambda,
	covariates = NULL,
	BC = NULL,
	GCV = FALSE,
	CPP_CODE = TRUE,
	GCVmethod = 2,
	nrealizations = 100,
	RNGstate = "",
	solver = "EigenLU",
	nprocessors = 1,
	PDE_parameters_constant = NULL,
	PDE_parameters_func = NULL)
{
  #################### Parameter Check #########################
  if(!is.null(locations))
  {
    if(any(is.na(locations)))
      stop("Missing values not admitted in 'locations'.")
    if(any(is.na(observations)))
      stop("Missing values not admitted in 'observations' when 'locations' are specified.")
  }
  
  if (is.null(observations)) 
    stop("observations required;  is NULL.")
  
  if (is.null(FEMbasis)) 
    stop("FEMbasis required;  is NULL.")
  if(class(FEMbasis)!= "FEMbasis")
    stop("'FEMbasis' is not class 'FEMbasis'")
  
  if (is.null(lambda)) 
    stop("lambda required;  is NULL.")
  
  if(!is.null(BC))
  {
    if (is.null(BC$BC_indices)) 
      stop("'BC_indices' required in BC;  is NULL.")
    if (is.null(BC$BC_values)) 
      stop("'BC_indices' required in BC;  is NULL.")
  }
  
  if (is.null(GCV)) 
    stop("GCV required;  is NULL.")
  if(!is.logical(GCV))
    stop("'GCV' is not logical")
  
  if (is.null(CPP_CODE)) 
    stop("CPP_CODE required;  is NULL.")
  if(!is.logical(CPP_CODE))
    stop("'CPP_CODE' is not logical")
  
  if (GCVmethod != 1 && GCVmethod != 2)
    stop("GCVmethod must be either 1(exact calculation) or 2(stochastic estimation)")

  if( !is.numeric(nrealizations) || nrealizations < 1)
    stop("nrealizations must be a positive integer")

  if ( !is.character(RNGstate))
    stop("Invalid RNGstate: it needs to be a character previously returned by a smoothing function")
  
  if (solver != "MumpsSparse" && solver != "EigenSparseLU")
    stop("Solver must be either \"MumpsSparse\" or \"EigenSparseLU\"")

  if (!is.numeric(nprocessors) || nprocessors < 1)
    stop("nprocessors must be a positive integer")

  if(!is.null(PDE_parameters_constant))
  {
    if (is.null(PDE_parameters_constant$K)) 
      stop("'K' required in PDE_parameters;  is NULL.")
    if (is.null(PDE_parameters_constant$b)) 
      stop("'b' required in PDE_parameters;  is NULL.")
    if (is.null(PDE_parameters_constant$c)) 
      stop("'c' required in PDE_parameters;  is NULL.")
  }
  
  if(!is.null(PDE_parameters_func))
  {
    if (is.null(PDE_parameters_func$K)) 
      stop("'K' required in PDE_parameters;  is NULL.")
    if(!is.function(PDE_parameters_func$K))
      stop("'K' in 'PDE_parameters' is not a function")
    if (is.null(PDE_parameters_func$b)) 
      stop("'b' required in PDE_parameters;  is NULL.")
    if(!is.function(PDE_parameters_func$b))
      stop("'b' in 'PDE_parameters' is not a function")
    if (is.null(PDE_parameters_func$c)) 
      stop("'c' required in PDE_parameters;  is NULL.")
    if(!is.function(PDE_parameters_func$c))
      stop("'c' in 'PDE_parameters' is not a function")
    if (is.null(PDE_parameters_func$u)) 
      stop("'u' required in PDE_parameters;  is NULL.")
    if(!is.function(PDE_parameters_func$u))
      stop("'u' in 'PDE_parameters' is not a function")
  }
}

checkSmoothingParametersSize<-function(
	locations = NULL,
	observations,
	FEMbasis,
	lambda,
	covariates = NULL,
	BC = NULL,
	GCV = FALSE,
	CPP_CODE = TRUE,
	PDE_parameters_constant = NULL,
	PDE_parameters_func = NULL)
{
  #################### Parameter Check #########################
  if(ncol(observations) != 1)
    stop("'observations' must be a column vector")
  if(nrow(observations) < 1)
    stop("'observations' must contain at least one element")
  if(!is.null(locations))
  {
    if(nrow(observations) > nrow(FEMbasis$mesh$nodes))
      stop("number of 'observations' is larger then the numer of 'noded' in the mesh")
  }
  if(!is.null(locations))
  {
    if(ncol(locations) != 2)
      stop("'locations' must be a 2-columns matrix;")
    if(nrow(locations) != nrow(observations))
      stop("'locations' and 'observations' have incompatible size;")
  }
  if(ncol(lambda) != 1)
    stop("'lambda' must be a column vector")
  if(nrow(lambda) < 1)
    stop("'lambda' must contain at least one element")
  if(!is.null(covariates))
  {
    if(nrow(covariates) != nrow(observations))
      stop("'covariates' and 'observations' have incompatible size;")
  }
  
  if(!is.null(BC))
  {
    if(ncol(BC$BC_indices) != 1)
      stop("'BC_indices' must be a column vector")
    if(ncol(BC$BC_values) != 1)
      stop("'BC_values' must be a column vector")
    if(nrow(BC$BC_indices) != nrow(BC$BC_values))
      stop("'BC_indices' and 'BC_values' have incompatible size;")
    if(sum(BC$BC_indices>nrow(nrow(FEMbasis$mesh$nodes))) > 0)
      stop("At least one index in 'BC_indices' larger then the numer of 'noded' in the mesh;")
  }
  
  if(!is.null(PDE_parameters_constant))
  {
    if(!all.equal(dim(PDE_parameters_constant$K), c(2,2)))
      stop("'K' in 'PDE_parameters must be a 2x2 matrix")
    if(!all.equal(dim(PDE_parameters_constant$b), c(2,1)))
      stop("'b' in 'PDE_parameters must be a column vector of size 2")
    if(!all.equal(dim(PDE_parameters_constant$c), c(1,1)))
      stop("'c' in 'PDE_parameters must be a double")
  }
  
  if(!is.null(PDE_parameters_func))
  {
    
    n_test_points = min(nrow(FEMbasis$mesh$nodes), 5)
    test_points = FEMbasis$mesh$nodes[1:n_test_points, ]
    
    try_K_func = PDE_parameters_func$K(test_points)
    try_b_func = PDE_parameters_func$b(test_points)
    try_c_func = PDE_parameters_func$c(test_points)
    try_u_func = PDE_parameters_func$u(test_points)
    
    if(!is.numeric(try_K_func))
      stop("Test on function 'K' in 'PDE_parameters' not passed; output is not numeric")
    if(!all.equal(dim(try_K_func), c(2,2,n_test_points)) )
      stop("Test on function 'K' in 'PDE_parameters' not passed; wrong size of the output")
    
    if(!is.numeric(try_b_func))
      stop("Test on function 'b' in 'PDE_parameters' not passed; output is not numeric")
    if(!all.equal(dim(try_b_func), c(2,n_test_points)))
      stop("Test on function 'b' in 'PDE_parameters' not passed; wrong size of the output")
    
    if(!is.numeric(try_c_func))
      stop("Test on function 'c' in 'PDE_parameters' not passed; output is not numeric")
    if(length(try_c_func) != n_test_points)
      stop("Test on function 'c' in 'PDE_parameters' not passed; wrong size of the output")
    
    if(!is.numeric(try_u_func))
      stop("Test on function 'u' in 'PDE_parameters' not passed; output is not numeric")
    if(length(try_u_func) != n_test_points)
      stop("Test on function 'u' in 'PDE_parameters' not passed; wrong size of the output")
  }
  
}
