/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                       */
/*    This file is part of the HiGHS linear optimization suite           */
/*                                                                       */
/*    Written and engineered 2008-2019 at the University of Edinburgh    */
/*                                                                       */
/*    Available as open-source under the MIT License                     */
/*                                                                       */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**@file lp_data/HighsUtils.cpp
 * @brief Class-independent utilities for HiGHS
 * @author Julian Hall, Ivet Galabova, Qi Huangfu and Michael Feldmeier
 */

#include "HConfig.h"
#include "io/HighsIO.h"
#include "lp_data/HighsLpUtils.h"
#include "lp_data/HighsModelUtils.h"
#include "util/HighsUtils.h"
#include "lp_data/HighsStatus.h"

HighsStatus checkLp(const HighsLp& lp) {
  // Check dimensions.
  if (lp.numCol_ <= 0 || lp.numRow_ <= 0)
    return HighsStatus::LpError;

  // Check vectors.
  if ((int)lp.colCost_.size() != lp.numCol_)
    return HighsStatus::LpError;

  if ((int)lp.colLower_.size() != lp.numCol_ ||
      (int)lp.colUpper_.size() != lp.numCol_)
    return HighsStatus::LpError;
  if ((int)lp.rowLower_.size() != lp.numRow_ ||
      (int)lp.rowUpper_.size() != lp.numRow_)
    return HighsStatus::LpError;

  for (int i = 0; i < lp.numRow_; i++)
    if (lp.rowLower_[i] < -HIGHS_CONST_INF || lp.rowUpper_[i] > HIGHS_CONST_INF)
      return HighsStatus::LpError;

  for (int j = 0; j < lp.numCol_; j++) {
    if (lp.colCost_[j] < -HIGHS_CONST_INF || lp.colCost_[j] > HIGHS_CONST_INF)
      return HighsStatus::LpError;

    if (lp.colLower_[j] < -HIGHS_CONST_INF || lp.colUpper_[j] > HIGHS_CONST_INF)
      return HighsStatus::LpError;
    if (lp.colLower_[j] > lp.colUpper_[j] + kBoundTolerance)
      return HighsStatus::LpError;
  }

  // Check matrix.
  if ((size_t)lp.nnz_ != lp.Avalue_.size())
    return HighsStatus::LpError;
  if (lp.nnz_ <= 0) return HighsStatus::LpError;
  if ((int)lp.Aindex_.size() != lp.nnz_)
    return HighsStatus::LpError;

  if ((int)lp.Astart_.size() != lp.numCol_ + 1)
    return HighsStatus::LpError;
  // Was lp.Astart_[i] >= lp.nnz_ (below), but this is wrong when the
  // last column is empty. Need to check as follows, and also check
  // the entry lp.Astart_[lp.numCol_] > lp.nnz_
  for (int i = 0; i < lp.numCol_; i++) {
    if (lp.Astart_[i] > lp.Astart_[i + 1] || lp.Astart_[i] > lp.nnz_ ||
        lp.Astart_[i] < 0)
      return HighsStatus::LpError;
  }
  if (lp.Astart_[lp.numCol_] > lp.nnz_ ||
      lp.Astart_[lp.numCol_] < 0)
      return HighsStatus::LpError;

  for (int k = 0; k < lp.nnz_; k++) {
    if (lp.Aindex_[k] < 0 || lp.Aindex_[k] >= lp.numRow_)
      return HighsStatus::LpError;
    if (lp.Avalue_[k] < -HIGHS_CONST_INF || lp.Avalue_[k] > HIGHS_CONST_INF)
      return HighsStatus::LpError;
  }

  return HighsStatus::OK;
}

HighsStatus assessLp(HighsLp& lp, const HighsOptions& options, bool normalise) {
  HighsStatus return_status = HighsStatus::NotSet;
  HighsStatus call_status;
  // Assess the LP dimensions and vector sizes, returning on error
  call_status = assessLpDimensions(lp);
  return_status = worse_status(call_status, return_status);
  if (return_status == HighsStatus::Error) return HighsStatus::LpError;

  // If the LP has no columns there is nothing left to test
  // NB assessLpDimensions returns HighsStatus::Error if lp.numCol_ < 0
  if (lp.numCol_ == 0) return HighsStatus::OK;

  // From here, any LP has lp.numCol_ > 0 and lp.Astart_[lp.numCol_] exists (as the number of nonzeros)
  assert(lp.numCol_ > 0);
  int lp_num_nz = lp.Astart_[lp.numCol_];

  // Assess the LP column costs
  call_status = assessCosts(0, lp.numCol_-1, &lp.colCost_[0], options.infinite_cost);
  return_status = worse_status(call_status, return_status);
  // Assess the LP column bounds
  call_status = assessBounds("Col", 0, lp.numCol_-1, &lp.colLower_[0], &lp.colUpper_[0], options.infinite_bound, normalise);
  return_status = worse_status(call_status, return_status);
  // Assess the LP row bounds
  call_status = assessBounds("Row", 0, lp.numRow_-1, &lp.rowLower_[0], &lp.rowUpper_[0], options.infinite_bound, normalise);
  return_status = worse_status(call_status, return_status);
  // Assess the LP matrix
  call_status = assessMatrix(lp.numRow_, 0, lp.numCol_-1, lp_num_nz, &lp.Astart_[0], &lp.Aindex_[0], &lp.Avalue_[0],
			      options.small_matrix_value, options.large_matrix_value, normalise);
  return_status = worse_status(call_status, return_status);
  if (return_status == HighsStatus::Error) return_status = HighsStatus::LpError;
  HighsLogMessage(HighsMessageType::INFO, "assess_lp returns HighsStatus = %s\n", HighsStatusToString(return_status).c_str());
  return return_status;
}

HighsStatus assessLpDimensions(const HighsLp& lp) {
  HighsStatus return_status = HighsStatus::NotSet;

  // Use error_found to track whether an error has been found in multiple tests
  bool error_found = false;

  // Set all these to true to avoid false asserts, since not all can be tested
  bool legal_num_row = true;
  bool legal_num_col = true;
  bool legal_num_nz = true;
  bool legal_col_cost_size = true;
  bool legal_col_lower_size = true;
  bool legal_col_upper_size = true;
  bool legal_row_lower_size = true;
  bool legal_row_upper_size = true;
  bool legal_matrix_start_size = true;
  bool legal_matrix_index_size = true;
  bool legal_matrix_value_size = true;

  // Don't expect the matrix_start_size to be legal if there are no columns
  bool check_matrix_start_size = lp.numCol_ > 0;

  // Assess column-related dimensions
  legal_num_col = lp.numCol_ >= 0;
  if (!legal_num_col) {
    HighsLogMessage(HighsMessageType::ERROR, "LP has illegal number of cols = %d\n", lp.numCol_);
    error_found = true;
  } else {
    // Check the size of the column vectors
    int col_cost_size = lp.colCost_.size();
    int col_lower_size = lp.colLower_.size();
    int col_upper_size = lp.colUpper_.size();
    int matrix_start_size = lp.Astart_.size();
    legal_col_cost_size = col_cost_size >= lp.numCol_;
    legal_col_lower_size = col_lower_size >= lp.numCol_;
    legal_col_upper_size = col_lower_size >= lp.numCol_;
    
    if (!legal_col_cost_size) {
      HighsLogMessage(HighsMessageType::ERROR, "LP has illegal colCost size = %d < %d\n", col_cost_size, lp.numCol_);
      error_found = true;
    }
    if (!legal_col_lower_size) {
      HighsLogMessage(HighsMessageType::ERROR, "LP has illegal colLower size = %d < %d\n", col_lower_size, lp.numCol_);
      error_found = true;
    }
    if (!legal_col_upper_size) {
      HighsLogMessage(HighsMessageType::ERROR, "LP has illegal colUpper size = %d < %d\n", col_upper_size, lp.numCol_);
      error_found = true;
    }
    if (check_matrix_start_size) {
      legal_matrix_start_size = matrix_start_size >= lp.numCol_+1;
      if (!legal_matrix_start_size) {
	HighsLogMessage(HighsMessageType::ERROR, "LP has illegal Astart size = %d < %d\n", matrix_start_size, lp.numCol_+1);
	error_found = true;
      }
    }
  }

  // Assess row-related dimensions
  legal_num_row = lp.numRow_ >= 0;
  assert(legal_num_row);
  if (!legal_num_row) {
    HighsLogMessage(HighsMessageType::ERROR, "LP has illegal number of rows = %d\n", lp.numRow_);
    error_found = true;
  } else {
    int row_lower_size = lp.rowLower_.size();
    int row_upper_size = lp.rowUpper_.size();
    legal_row_lower_size = row_lower_size >= lp.numRow_;
    legal_row_upper_size = row_lower_size >= lp.numRow_;
    if (!legal_row_lower_size) {
      HighsLogMessage(HighsMessageType::ERROR, "LP has illegal rowLower size = %d < %d\n", row_lower_size, lp.numRow_);
      error_found = true;
    }
    if (!legal_row_upper_size) {
      HighsLogMessage(HighsMessageType::ERROR, "LP has illegal rowUpper size = %d < %d\n", row_upper_size, lp.numRow_);
      error_found = true;
    }
  }

  // Assess matrix-related dimensions
  if (check_matrix_start_size) {
    int lp_num_nz = lp.Astart_[lp.numCol_];
    legal_num_nz = lp_num_nz >= 0;
    if (!legal_num_nz) {
      HighsLogMessage(HighsMessageType::ERROR, "LP has illegal number of nonzeros = %d\n", lp_num_nz);
      error_found = true;
    } else {
      int matrix_index_size = lp.Aindex_.size();
      int matrix_value_size = lp.Avalue_.size();
      legal_matrix_index_size = matrix_index_size >= lp_num_nz;
      legal_matrix_value_size = matrix_value_size >= lp_num_nz;
      if (!legal_matrix_index_size) {
	HighsLogMessage(HighsMessageType::ERROR, "LP has illegal Aindex size = %d < %d\n", matrix_index_size, lp_num_nz);
	error_found = true;
      }
      if (!legal_matrix_value_size) {
	HighsLogMessage(HighsMessageType::ERROR, "LP has illegal Avalue size = %d < %d\n", matrix_value_size, lp_num_nz);
	error_found = true;
      }
    }
  }
  assert(legal_num_row);
  assert(legal_num_col);
  assert(legal_num_nz);
  assert(legal_col_cost_size);
  assert(legal_col_lower_size);
  assert(legal_col_upper_size);
  assert(legal_row_lower_size);
  assert(legal_row_upper_size);
  assert(legal_matrix_start_size);
  assert(legal_matrix_index_size);
  assert(legal_matrix_value_size);

  if (error_found) return_status = HighsStatus::Error;
  else return_status = HighsStatus::OK;

  return return_status;  
}

HighsStatus assessCosts(int Xfrom_col, int Xto_col, double* XcolCost, double infinite_cost) {
  assert(Xfrom_col >= 0);
  assert(Xto_col >= 0);
  assert(Xfrom_col <= Xto_col+1);
  if (Xfrom_col > Xto_col) return HighsStatus::OK;

  HighsStatus return_status = HighsStatus::NotSet;
  bool error_found = false;
  for (int col = Xfrom_col; col <= Xto_col; col++) {
    double absCost = abs(XcolCost[col]);
    bool legalCost = absCost < infinite_cost;
    assert(legalCost);
    if (!legalCost) {
      HighsLogMessage(HighsMessageType::ERROR, "Col  %12d has |cost| of %12g >= %12g",  col, absCost, infinite_cost);
      error_found = true;
    }
  }
  if (error_found) return_status = HighsStatus::Error;
  else return_status = HighsStatus::OK;
    
  return return_status;
}

HighsStatus assessBounds(const char* type, int Xfrom_ix, int Xto_ix, double* XLower, double* XUpper, double infinite_bound, bool normalise) {
  assert(Xfrom_ix >= 0);
  assert(Xto_ix >= 0);
  assert(Xfrom_ix <= Xto_ix+1);
  if (Xfrom_ix > Xto_ix) return HighsStatus::OK;

  HighsStatus return_status = HighsStatus::NotSet;
  bool error_found = false;
  bool warning_found = false;
  bool info_found = false;
  int num_infinite_lower_bound = 0;
  int num_infinite_upper_bound = 0;
  for (int ix = Xfrom_ix; ix <= Xto_ix; ix++) {
    if (!highs_isInfinity(-XLower[ix])) {
      // Check whether a finite lower bound will be treated as -Infinity      
      bool infinite_lower_bound = XLower[ix] <= -infinite_bound;
      if (infinite_lower_bound) {
	if (normalise) XLower[ix] = -HIGHS_CONST_INF;
	num_infinite_lower_bound++;
      }
    }
    if (!highs_isInfinity(XUpper[ix])) {
      // Check whether a finite upper bound will be treated as Infinity      
      bool infinite_upper_bound = XUpper[ix] >= infinite_bound;
      if (infinite_upper_bound) {
	if (normalise) XUpper[ix] = HIGHS_CONST_INF;
	num_infinite_upper_bound++;
      }
    }
    // Check that the lower bound does not exceed the upper bound
    bool legalLowerUpperBound = XLower[ix] <= XUpper[ix];
    if (!legalLowerUpperBound) {
      // Leave inconsistent bounds to be used to deduce infeasibility
      HighsLogMessage(HighsMessageType::WARNING, "%3s  %12d has inconsistent bounds [%12g, %12g]", type, ix, XLower[ix], XUpper[ix]);
      warning_found = true;
    } else {
      // Check that the lower bound is not as much as +Infinity
      bool legalLowerBound = XLower[ix] < infinite_bound;
      assert(legalLowerBound);
      if (!legalLowerBound) {
	HighsLogMessage(HighsMessageType::ERROR, "%3s  %12d has lower bound of %12g >= %12g", type, ix, XLower[ix], infinite_bound);
	error_found = true;
      }
      // Check that the upper bound is not as little as -Infinity
      bool legalUpperBound = XUpper[ix] > -infinite_bound;
      assert(legalUpperBound);
      if (!legalUpperBound) {
	HighsLogMessage(HighsMessageType::ERROR, "%3s  %12d has upper bound of %12g <= %12g", type, ix, XUpper[ix], -infinite_bound);
	error_found = true;
      }
    }
  }
  if (normalise) {
    if (num_infinite_lower_bound) {
      HighsLogMessage(HighsMessageType::INFO, "%3ss:%12d lower bounds exceeding %12g are treated as -Infinity", type, num_infinite_lower_bound, -infinite_bound);
      info_found = true;
    }
    if (num_infinite_upper_bound) {
      HighsLogMessage(HighsMessageType::INFO, "%3ss:%12d upper bounds exceeding %12g are treated as +Infinity", type, num_infinite_upper_bound, infinite_bound);
      info_found = true;
    }
  }
  if (error_found) return_status = HighsStatus::Error;
  else if (warning_found) return_status = HighsStatus::Warning;
  else if (info_found) return_status = HighsStatus::Info;
  else return_status = HighsStatus::OK;
    
  return return_status;
}

HighsStatus assessMatrix(int Xdim, int Xfrom_ix, int Xto_ix, int Xnum_nz, int* Xstart, int* Xindex, double* Xvalue,
			 double small_matrix_value, double large_matrix_value, bool normalise) {
  assert(Xdim >= 0);
  assert(Xfrom_ix >= 0);
  assert(Xto_ix >= 0);
  assert(Xfrom_ix <= Xto_ix+1);
  if (Xfrom_ix > Xto_ix) return HighsStatus::OK;

  HighsStatus return_status = HighsStatus::NotSet;
  bool error_found = false;
  bool warning_found = false;

  // Warn the user if the first start is not zero
  int fromEl = Xstart[0];
  if (fromEl != 0) {
      HighsLogMessage(HighsMessageType::WARNING, "Matrix starts do not begin with 0");
      warning_found = true;
  }
  // Assess the starts
  // Set up previous_start for a fictitious previous empty packed vector
  int previous_start = std::max(0, Xstart[Xfrom_ix]);
  for (int ix = Xfrom_ix; ix <= Xto_ix; ix++) {
    int this_start = Xstart[ix];
    bool this_start_too_small = this_start < previous_start;
    assert(!this_start_too_small);
    if (this_start_too_small) {
      HighsLogMessage(HighsMessageType::ERROR, "Matrix packed vector %d has illegal start of %d < %d = previous start", ix, this_start, previous_start);
      return HighsStatus::Error;
    }
    bool this_start_too_big = this_start > Xnum_nz;
    if (this_start_too_big) {
      HighsLogMessage(HighsMessageType::ERROR, "Matrix packed vector %d has illegal start of %d > %d = number of nonzeros", ix, this_start, Xnum_nz);
      return HighsStatus::Error;
    }
  }

  // Assess the indices and values
  // Count the number of acceptable indices/values
  int new_num_nz = Xstart[Xfrom_ix];
  int num_small_values = 0;
  // Set up a zeroed vector to detect duplicate indices
  vector<int> check_vector;
  if (Xdim > 0) check_vector.assign(Xdim, 0);
  for (int ix = Xfrom_ix; ix <= Xto_ix; ix++) {
    int from_el = Xstart[ix];
    int to_el;
    if (ix < Xto_ix) {
      to_el = Xstart[ix+1]-1;
    } else {
      to_el = Xnum_nz-1;
    }
    if (normalise) {
      // Account for any index-value pairs removed so far
      Xstart[ix] = new_num_nz;
    }
    for (int el = from_el; el <= to_el; el++) {
      int component = Xindex[el];
      // Check that the index is non-negative
      bool legal_component = component >= 0;
      if (!legal_component) {
	HighsLogMessage(HighsMessageType::ERROR, "Matrix packed vector %d, entry %d, is illegal index %d", ix, el, component);
	assert(legal_component);
	return HighsStatus::Error;
      }
      // Check that the index does not exceed the vector dimension
      legal_component = component < Xdim;
      if (!legal_component) {
	HighsLogMessage(HighsMessageType::ERROR, "Matrix packed vector %d, entry %d, is illegal index %12d >= %g", ix, el, component, Xdim);
	assert(legal_component);
	return HighsStatus::Error;
      }
      // Check that the index has not already ocurred
      legal_component = check_vector[component] == 0;
      if (!legal_component) {
	HighsLogMessage(HighsMessageType::ERROR, "Matrix packed vector %d, entry %d, is duplicate index %d", ix, el, component);	  
	assert(legal_component);
	return HighsStatus::Error;
      }
      // Check that the value is not too large
      double abs_value = fabs(Xvalue[el]);
      bool large_value = abs_value >= large_matrix_value;
      if (large_value) {
	HighsLogMessage(HighsMessageType::ERROR, "Matrix packed vector %d, entry %d, is large value |%g| >= %g", ix, el, abs_value, large_matrix_value);	  
	assert(!large_value);
	return HighsStatus::Error;
      }
      bool ok_value = abs_value > small_matrix_value;
      if (!ok_value) {
#ifdef HiGHSDEV
#endif
	HighsLogMessage(HighsMessageType::WARNING, "Matrix packed vector %d, entry %d, is small value |%g| <= %g", ix, el, abs_value, small_matrix_value);	  
	num_small_values++;
      }
      if (!ok_value && normalise) {
	Xindex[new_num_nz] = Xindex[el];
	Xvalue[new_num_nz] = Xvalue[el];
      } else {
	new_num_nz++;
      }
    }
    // Zero check_vector
    for (int el = Xstart[ix]; el <= new_num_nz-1; el++) check_vector[Xindex[el]] = 0;
#ifdef HiGHSDEV
    // Check zeroing of check vector
    for (int component = 0; component < Xdim; component++) {
      if (check_vector[component]) error_found;
    }
    if (error_found) HighsLogMessage(HighsMessageType::ERROR, "assessMatrix: check_vector not zeroed");
#endif
  }
  if (num_small_values) {
    HighsLogMessage(HighsMessageType::WARNING, "Matrix packed vector contains %d values less than %g in magnitude: ignored", num_small_values, small_matrix_value);	  
    warning_found = true;
  }
  if (error_found) return_status = HighsStatus::Error;
  else if (warning_found) return_status = HighsStatus::Warning;
  else return_status = HighsStatus::OK;

  return return_status;

}


HighsStatus add_lp_cols(HighsLp& lp,
			int XnumNewCol, const double *XcolCost, const double *XcolLower,  const double *XcolUpper,
			int XnumNewNZ, const int *XAstart, const int *XAindex, const double *XAvalue,
			const HighsOptions& options, const bool force) {
  HighsStatus return_status = HighsStatus::NotSet;
  HighsStatus call_status = append_lp_cols(lp, XnumNewCol, XcolCost, XcolLower, XcolUpper,
					   XnumNewNZ, XAstart, XAindex, XAvalue,
					   options, force);
  lp.numCol_ += XnumNewCol;
  return_status = call_status;
  return return_status;
}

HighsStatus append_lp_cols(HighsLp& lp,
			   int XnumNewCol, const double *XcolCost, const double *XcolLower,  const double *XcolUpper,
			   int XnumNewNZ, const int *XAstart, const int *XAindex, const double *XAvalue,
			   const HighsOptions& options, const bool force) {
  HighsStatus return_status = HighsStatus::NotSet;
  int newNumCol = lp.numCol_ + XnumNewCol;
  // Assess the bounds and matrix indices, returning on error unless addition is forced
  bool normalise = false;
  HighsStatus call_status;
  call_status = assessBounds("Col", 0, XnumNewCol-1, (double*)XcolLower, (double*)XcolUpper, options.infinite_bound, normalise);
  return_status = worse_status(call_status, return_status);

  call_status = assessMatrix(lp.numRow_, 0, XnumNewCol-1, XnumNewNZ, (int*)XAstart, (int*)XAindex, (double*)XAvalue,
			      options.small_matrix_value, options.large_matrix_value, normalise);
  return_status = worse_status(call_status, return_status);
  if (return_status == HighsStatus::Error && !force) return return_status;

  // Append the columns to the LP vectors and matrix
  append_cols_to_lp_vectors(lp, XnumNewCol, XcolCost, XcolLower, XcolUpper);
  append_cols_to_lp_matrix(lp, XnumNewCol, XnumNewNZ, XAstart, XAindex, XAvalue);

  // Normalise the new LP column bounds and matrix columns
  normalise = true;
  call_status = assessBounds("Col", lp.numCol_, newNumCol-1, &lp.colLower_[0], &lp.colUpper_[0], options.infinite_bound, normalise);
  return_status = worse_status(call_status, return_status);
  call_status = normalise_lp_matrix(lp, lp.numCol_, newNumCol, options.small_matrix_value, options.large_matrix_value);
  return_status = worse_status(call_status, return_status);
  return return_status;
}

HighsStatus append_cols_to_lp_vectors(HighsLp &lp, int XnumNewCol,
			       const double*XcolCost, const double *XcolLower, const double *XcolUpper) {
  assert(XnumNewCol >= 0);
  if (XnumNewCol == 0) return HighsStatus::OK;
  int newNumCol = lp.numCol_ + XnumNewCol;
  lp.colCost_.resize(newNumCol);
  lp.colLower_.resize(newNumCol);
  lp.colUpper_.resize(newNumCol);

  for (int col = 0; col < XnumNewCol; col++) {
    lp.colCost_[lp.numCol_ + col] = XcolCost[col];
    lp.colLower_[lp.numCol_ + col] = XcolLower[col];
    lp.colUpper_[lp.numCol_ + col] = XcolUpper[col];
  }
}

HighsStatus normalise_col_bounds(HighsLp& lp, int XfromCol, int XtoCol, const double infinite_bound) {
  HighsStatus return_status = HighsStatus::NotSet;
  assert(XfromCol >= 0);
  assert(XtoCol < lp.numCol_);
  assert(XfromCol <= XtoCol);
  int numChangedLowerBounds = 0;
  int numChangedUpperBounds = 0;
  for (int col = XfromCol; col <= XtoCol; col++) {
    if (lp.colLower_[col] <= -infinite_bound) {
      lp.colLower_[col] = -HIGHS_CONST_INF;
      numChangedLowerBounds++;
    }
    if (lp.colUpper_[col] >= infinite_bound) {
      lp.colUpper_[col] = HIGHS_CONST_INF;
      numChangedUpperBounds++;
    }
  }
  if (numChangedLowerBounds)
    HighsLogMessage(HighsMessageType::WARNING, "%12d col lower bounds below %12g interpreted as -Infinity",
		    numChangedLowerBounds, -infinite_bound);
  if (numChangedUpperBounds)
    HighsLogMessage(HighsMessageType::WARNING, "%12d col upper bounds above %12g interpreted as +Infinity",
		    numChangedUpperBounds, infinite_bound);
  int numChangedBounds = numChangedLowerBounds + numChangedUpperBounds;
  if (numChangedBounds)
    return_status = HighsStatus::Warning;
  else
    return_status = HighsStatus::OK;
    
  return return_status;
}

HighsStatus add_lp_rows(HighsLp& lp,
		int XnumNewRow, const double *XrowLower,  const double *XrowUpper,
		int XnumNewNZ, const int *XARstart, const int *XARindex, const double *XARvalue,
		const HighsOptions& options, const bool force) {
  HighsStatus return_status = HighsStatus::NotSet;
  HighsStatus call_status = append_lp_rows(lp, XnumNewRow, XrowLower, XrowUpper,
				  XnumNewNZ, XARstart, XARindex, XARvalue,
				  options, force);
  lp.numRow_ += XnumNewRow;
  return_status = call_status;
  return return_status;
}

HighsStatus append_lp_rows(HighsLp& lp,
		   int XnumNewRow, const double *XrowLower,  const double *XrowUpper,
		   int XnumNewNZ, const int *XARstart, const int *XARindex, const double *XARvalue,
		   const HighsOptions& options, const bool force) {
  HighsStatus return_status = HighsStatus::NotSet;
  int newNumRow = lp.numRow_ + XnumNewRow;
  // Assess the bounds and matrix indices, returning on error unless addition is forced
  bool normalise = false;
  HighsStatus call_status = assessBounds("Row", 0, XnumNewRow-1, (double*)XrowLower, (double*)XrowUpper, options.infinite_bound, normalise);
  return_status = worse_status(call_status, return_status);

  if (return_status == HighsStatus::Error && !force) return return_status;

  append_rows_to_lp_vectors(lp, XnumNewRow, XrowLower, XrowUpper);
  normalise = true;
  call_status = normalise_row_bounds(lp, lp.numRow_, newNumRow, options.infinite_bound);
  return_status = worse_status(call_status, return_status);
  return return_status;
}

HighsStatus append_rows_to_lp_vectors(HighsLp &lp, int XnumNewRow,
			       const double *XrowLower, const double *XrowUpper) {
  assert(XnumNewRow >= 0);
  if (XnumNewRow == 0) return HighsStatus::OK;
  int newNumRow = lp.numRow_ + XnumNewRow;
  lp.rowLower_.resize(newNumRow);
  lp.rowUpper_.resize(newNumRow);

  for (int row = 0; row < XnumNewRow; row++) {
    lp.rowLower_[lp.numRow_ + row] = XrowLower[row];
    lp.rowUpper_[lp.numRow_ + row] = XrowUpper[row];
  }
}

HighsStatus normalise_row_bounds(HighsLp& lp, int XfromRow, int XtoRow, double infinite_bound) {
  HighsStatus return_status = HighsStatus::NotSet;
  assert(XfromRow >= 0);
  assert(XtoRow < lp.numRow_);
  assert(XfromRow <= XtoRow);
  int numChangedLowerBounds = 0;
  int numChangedUpperBounds = 0;
  for (int row = XfromRow; row <= XtoRow; row++) {
    if (lp.rowLower_[row] <= -infinite_bound) {
      lp.rowLower_[row] = -HIGHS_CONST_INF;
      numChangedLowerBounds++;
    }
    if (lp.rowUpper_[row] >= infinite_bound) {
      lp.rowUpper_[row] = HIGHS_CONST_INF;
      numChangedUpperBounds++;
    }
  }
  if (numChangedLowerBounds)
    HighsLogMessage(HighsMessageType::WARNING, "%12d row lower bounds below %12g interpreted as -Infinity",
		    numChangedLowerBounds, -infinite_bound);
  if (numChangedUpperBounds)
    HighsLogMessage(HighsMessageType::WARNING, "%12d row upper bounds above %12g interpreted as +Infinity",
		    numChangedUpperBounds, infinite_bound);
  int numChangedBounds = numChangedLowerBounds + numChangedUpperBounds;
  if (numChangedBounds)
    return_status = HighsStatus::Warning;
  else
    return_status = HighsStatus::OK;
    
  return return_status;
}

HighsStatus append_cols_to_lp_matrix(HighsLp &lp, int XnumNewCol,
			      int XnumNewNZ, const int *XAstart, const int *XAindex, const double *XAvalue) {
  assert(XnumNewCol >= 0);
  assert(XnumNewNZ >= 0);
  if (XnumNewCol == 0) return HighsStatus::OK;
  int newNumCol = lp.numCol_ + XnumNewCol;
  lp.Astart_.resize(newNumCol + 1);
  for (int col = 0; col < XnumNewCol; col++) {
    lp.Astart_[lp.numCol_ + col + 1] = lp.Astart_[lp.numCol_];
  }
  
  // Determine the current number of nonzeros
  int currentNumNZ = lp.Astart_[lp.numCol_];
  
  // Determine the new number of nonzeros and resize the column-wise matrix
  // arrays
  int newNumNZ = currentNumNZ + XnumNewNZ;
  lp.Aindex_.resize(newNumNZ);
  lp.Avalue_.resize(newNumNZ);
  
  // Append the new columns
  for (int col = 0; col < XnumNewCol; col++) {
    lp.Astart_[lp.numCol_ + col] = XAstart[col] + currentNumNZ;
  }
  lp.Astart_[lp.numCol_ + XnumNewCol] = newNumNZ;
  
  for (int el = 0; el < XnumNewNZ; el++) {
    int row = XAindex[el];
    assert(row >= 0);
    assert(row < lp.numRow_);
    lp.Aindex_[currentNumNZ + el] = row;
    lp.Avalue_[currentNumNZ + el] = XAvalue[el];
  }
}

HighsStatus append_rows_to_lp_matrix(HighsLp &lp, int XnumNewRow,
			      int XnumNewNZ, const int *XARstart, const int *XARindex, const double *XARvalue) {
  assert(XnumNewRow >= 0);
  assert(XnumNewNZ >= 0);
  // Check that nonzeros aren't being appended to a matrix with no columns
  assert(XnumNewNZ == 0 || lp.numCol_ > 0);
  if (XnumNewRow == 0) return HighsStatus::OK;
  int newNumRow = lp.numRow_ + XnumNewRow;

  // NB SCIP doesn't have XARstart[XnumNewRow] defined, so have to use XnumNewNZ for last
  // entry
  if (XnumNewNZ == 0) return HighsStatus::OK;
  int currentNumNZ = lp.Astart_[lp.numCol_];
  vector<int> Alength;
  Alength.assign(lp.numCol_, 0);
  for (int el = 0; el < XnumNewNZ; el++) {
    int col = XARindex[el];
    //      printf("El %2d: appending entry in column %2d\n", el, col); 
    assert(col >= 0);
    assert(col < lp.numCol_);
    Alength[col]++;
  }
  // Determine the new number of nonzeros and resize the column-wise matrix arrays
  int newNumNZ = currentNumNZ + XnumNewNZ;
  lp.Aindex_.resize(newNumNZ);
  lp.Avalue_.resize(newNumNZ);

  // Append the new rows
  // Shift the existing columns to make space for the new entries
  int nwEl = newNumNZ;
  for (int col = lp.numCol_ - 1; col >= 0; col--) {
    // printf("Column %2d has additional length %2d\n", col, Alength[col]);
    int Astart_Colp1 = nwEl;
    nwEl -= Alength[col];
    // printf("Shift: nwEl = %2d\n", nwEl);
    for (int el = lp.Astart_[col + 1] - 1; el >= lp.Astart_[col]; el--) {
      nwEl--;
      // printf("Shift: Over-writing lp.Aindex_[%2d] with lp.Aindex_[%2d]=%2d\n",
      // nwEl, el, lp.Aindex_[el]);
      lp.Aindex_[nwEl] = lp.Aindex_[el];
      lp.Avalue_[nwEl] = lp.Avalue_[el];
    }
    lp.Astart_[col + 1] = Astart_Colp1;
  }
  // printf("After shift: nwEl = %2d\n", nwEl);
  assert(nwEl == 0);
  // util_reportColMtx(lp.numCol_, lp.Astart_, lp.Aindex_, lp.Avalue_);

  // Insert the new entries
  for (int row = 0; row < XnumNewRow; row++) {
    int fEl = XARstart[row];
    int lEl = (row < XnumNewRow - 1 ? XARstart[row + 1] : XnumNewNZ) - 1;
    for (int el = fEl; el <= lEl; el++) {
      int col = XARindex[el];
      nwEl = lp.Astart_[col + 1] - Alength[col];
      Alength[col]--;
      // printf("Insert: row = %2d; col = %2d; lp.Astart_[col+1]-Alength[col] =
      // %2d; Alength[col] = %2d; nwEl = %2d\n", row, col,
      // lp.Astart_[col+1]-Alength[col], Alength[col], nwEl);
      assert(nwEl >= 0);
      assert(el >= 0);
      // printf("Insert: Over-writing lp.Aindex_[%2d] with lp.Aindex_[%2d]=%2d\n",
      // nwEl, el, lp.Aindex_[el]);
      lp.Aindex_[nwEl] = lp.numRow_ + row;
      lp.Avalue_[nwEl] = XARvalue[el];
    }
  }
}

HighsStatus normalise_lp_matrix(HighsLp& lp, int XfromCol, int XtoCol, double small_matrix_value, double large_matrix_value) {
  HighsStatus return_status = HighsStatus::NotSet;
  assert(XfromCol >= 0);
  assert(XtoCol < lp.numCol_);
  assert(XfromCol <= XtoCol);
  assert(small_matrix_value >=0);
  int numRemovedValues = 0;
  int numTrueNZ = lp.Astart_[XfromCol];
  for (int col = XfromCol; col <= XtoCol; col++) {
    int fromEl = lp.Astart_[col];
    lp.Astart_[col] = numTrueNZ;
    for (int el = fromEl; el < lp.Astart_[col+1]; el++) {
      if (fabs(lp.Avalue_[el]) <= small_matrix_value) {
	numRemovedValues++;
      } else {
	lp.Aindex_[numTrueNZ] = lp.Aindex_[el];
	lp.Avalue_[numTrueNZ] = lp.Avalue_[el];
	numTrueNZ++;
      }
    }
  }
  if (numRemovedValues) {
    for (int col = XtoCol+1; col < lp.numCol_; col++) {
      int fromEl = lp.Astart_[col];
      lp.Astart_[col] = numTrueNZ;
	for (int el = fromEl; el < lp.Astart_[col+1]; el++) {
	  lp.Aindex_[numTrueNZ] = lp.Aindex_[el];
	  lp.Avalue_[numTrueNZ] = lp.Avalue_[el];
	  numTrueNZ++;
	}
    }
    lp.Astart_[lp.numCol_] = numTrueNZ;
    lp.nnz_ = numTrueNZ;
    HighsLogMessage(HighsMessageType::WARNING, "%12d matrix values less than %12g removed",
		    numRemovedValues, small_matrix_value);
    return_status = HighsStatus::Warning;
  } else
    return_status = HighsStatus::OK;
    
  return return_status;
}

HighsStatus normalise_lp_row_matrix(int XnumCol, int XnumRow, int XnumNZ, int* XARstart, int* XARindex, double* XARvalue, double small_matrix_value, double large_matrix_value) {
  HighsStatus return_status = HighsStatus::NotSet;
  assert(XnumCol >= 0);
  assert(XnumRow >= 0);
  assert(XnumNZ >= 0);
  assert(small_matrix_value >=0);
  if (XnumRow == 0) return HighsStatus::OK;
  if (XnumNZ == 0) return HighsStatus::OK;
  int numRemovedValues = 0;
  int numTrueNZ = 0;
  for (int row = 0; row < XnumRow; row++) {
    int fromEl = XARstart[row];
    XARstart[row] = numTrueNZ;
    for (int el = fromEl; el < XARstart[row+1]; el++) {
      if (fabs(XARvalue[el]) <= small_matrix_value) {
	numRemovedValues++;
      } else {
	XARindex[numTrueNZ] = XARindex[el];
	XARvalue[numTrueNZ] = XARvalue[el];
	numTrueNZ++;
      }
    }
  }
  XARstart[XnumRow] = numTrueNZ;
  if (numRemovedValues) {
    HighsLogMessage(HighsMessageType::WARNING, "%12d matrix values less than %12g removed",
		    numRemovedValues, small_matrix_value);
    return_status = HighsStatus::Warning;
  } else
    return_status = HighsStatus::OK;

  return return_status;
}


HighsStatus delete_lp_cols(HighsLp &lp, int XfromCol, int XtoCol) {
}

HighsStatus delete_lp_col_set(HighsLp &lp, int XnumCol, int* XcolSet) {
}

HighsStatus delete_cols_from_lp_vectors(HighsLp &lp, int XfromCol, int XtoCol) {
  assert(XfromCol >= 0);
  assert(XtoCol < lp.numCol_);
  assert(XfromCol <= XtoCol);

  int numDeleteCol = XtoCol - XfromCol + 1;
  if (numDeleteCol == 0 || numDeleteCol == lp.numCol_) return HighsStatus::OK;
  //
  // Trivial case is XtoCol = lp.numCol_-1, in which case no columns
  // need be shifted. However, this implies lp.numCol_-numDeleteCol =
  // XfromCol, in which case the loop is vacuous
  for (int col = XfromCol; col < lp.numCol_ - numDeleteCol; col++) {
    lp.colCost_[col] = lp.colCost_[col + numDeleteCol];
    lp.colLower_[col] = lp.colLower_[col + numDeleteCol];
    lp.colUpper_[col] = lp.colUpper_[col + numDeleteCol];
  }
}

HighsStatus delete_cols_from_lp_matrix(HighsLp &lp, int XfromCol, int XtoCol) {
  assert(XfromCol >= 0);
  assert(XtoCol < lp.numCol_);
  assert(XfromCol <= XtoCol);

  int numDeleteCol = XtoCol - XfromCol + 1;
  if (numDeleteCol == 0 || numDeleteCol == lp.numCol_) return HighsStatus::OK;
  //
  // Trivial case is XtoCol = lp.numCol_-1, in which case no columns need be shifted
  // and the loops are vacuous
  int elOs = lp.Astart_[XfromCol];
  int numDeleteEl = lp.Astart_[XtoCol + 1] - elOs;
  for (int el = lp.Astart_[XtoCol + 1]; el < lp.Astart_[lp.numCol_]; el++) {
    lp.Aindex_[el - numDeleteEl] = lp.Aindex_[el];
    lp.Avalue_[el - numDeleteEl] = lp.Avalue_[el];
  }
  for (int col = XfromCol; col <= lp.numCol_ - numDeleteCol; col++) {
    lp.Astart_[col] = lp.Astart_[col + numDeleteCol] - numDeleteEl;
  }

}

HighsStatus delete_col_set_from_lp_vectors(HighsLp &lp, int XnumCol, int* XcolSet) {
}

HighsStatus delete_col_set_from_lp_matrix(HighsLp &lp, int XnumCol, int* XcolSet) {
}

HighsStatus delete_lp_rows(HighsLp &lp, int XfromRow, int XtoRow) {
}

HighsStatus delete_lp_row_set(HighsLp &lp, int XnumRow, int* XrowSet) {
}

HighsStatus delete_rows_from_lp_vectors(HighsLp &lp, int XfromRow, int XtoRow) {
  assert(XfromRow >= 0);
  assert(XtoRow < lp.numRow_);
  assert(XfromRow <= XtoRow);

  int numDeleteRow = XtoRow - XfromRow + 1;
  if (numDeleteRow == 0 || numDeleteRow == lp.numRow_) return HighsStatus::OK;
  //
  // Trivial case is XtoRow = lp.numRow_-1, in which case no rows
  // need be shifted. However, this implies lp.numRow_-numDeleteRow =
  // XfromRow, in which case the loop is vacuous
  for (int row = XfromRow; row < lp.numRow_ - numDeleteRow; row++) {
    lp.rowLower_[row] = lp.rowLower_[row + numDeleteRow];
    lp.rowUpper_[row] = lp.rowUpper_[row + numDeleteRow];
  }
}

HighsStatus delete_rows_from_lp_matrix(HighsLp &lp, int XfromRow, int XtoRow) {
  assert(XfromRow >= 0);
  assert(XtoRow < lp.numRow_);
  assert(XfromRow <= XtoRow);

  int numDeleteRow = XtoRow - XfromRow + 1;
  if (numDeleteRow == 0 || numDeleteRow == lp.numRow_) return HighsStatus::OK;

  int nnz = 0;
  for (int col = 0; col < lp.numCol_; col++) {
    int fmEl = lp.Astart_[col];
    lp.Astart_[col] = nnz;
    for (int el = fmEl; el < lp.Astart_[col + 1]; el++) {
      int row = lp.Aindex_[el];
      if (row < XfromRow || row > XtoRow) {
	if (row < XfromRow) {
	  lp.Aindex_[nnz] = row;
	} else {
	  lp.Aindex_[nnz] = row - numDeleteRow;
	}
	lp.Avalue_[nnz] = lp.Avalue_[el];
	nnz++;
      }
    }
  }
  lp.Astart_[lp.numCol_] = nnz;
}

HighsStatus delete_row_set_from_lp_vectors(HighsLp &lp, int XnumRow, int* XrowSet) {
}

HighsStatus delete_row_set_from_lp_matrix(HighsLp &lp, int XnumRow, int* XrowSet) {
}

HighsStatus change_lp_matrix_coefficient(HighsLp &lp, int Xrow, int Xcol, const double XnewValue) {
  int changeElement = -1;
  for (int el = lp.Astart_[Xcol]; el < lp.Astart_[Xcol + 1]; el++) {
    // printf("Column %d: Element %d is row %d. Is it %d?\n", Xcol, el, lp.Aindex_[el], Xrow);
    if (lp.Aindex_[el] == Xrow) {
      changeElement = el;
      break;
    }
  }
  if (changeElement < 0) {
    //    printf("util_changeCoeff: Cannot find row %d in column %d\n", Xrow, Xcol);
    changeElement = lp.Astart_[Xcol + 1];
    int newNumNZ = lp.Astart_[lp.numCol_] + 1;
    //    printf("model.util_changeCoeff: Increasing Nnonz from %d to %d\n",
    //    lp.Astart_[lp.numCol_], newNumNZ);
    lp.Aindex_.resize(newNumNZ);
    lp.Avalue_.resize(newNumNZ);
    for (int i = Xcol + 1; i <= lp.numCol_; i++) lp.Astart_[i]++;
    for (int el = newNumNZ - 1; el > changeElement; el--) {
      lp.Aindex_[el] = lp.Aindex_[el - 1];
      lp.Avalue_[el] = lp.Avalue_[el - 1];
    }
  }
  lp.Aindex_[changeElement] = Xrow;
  lp.Avalue_[changeElement] = XnewValue;
}

void getLpCosts(
		const HighsLp& lp,
		int firstcol,
		int lastcol,
		double* XcolCost
		) {
  assert(0 <= firstcol);
  assert(firstcol <= lastcol);
  assert(lastcol < lp.numCol_);
  for (int col = firstcol; col <= lastcol; ++col) XcolCost[col - firstcol] = lp.colCost_[col];
}

void getLpColBounds(const HighsLp& lp,
		    int firstcol,
		    int lastcol,
		    double* XcolLower,
		    double* XcolUpper) {
  assert(0 <= firstcol);
  assert(firstcol <= lastcol);
  assert(lastcol < lp.numCol_);
  for (int col = firstcol; col <= lastcol; ++col) {
    if (XcolLower != NULL) XcolLower[col - firstcol] = lp.colLower_[col];
    if (XcolUpper != NULL) XcolUpper[col - firstcol] = lp.colUpper_[col];
  }
}

void getLpRowBounds(const HighsLp& lp,
		    int firstrow,
		    int lastrow,
		    double* XrowLower,
		    double* XrowUpper) {
  assert(0 <= firstrow);
  assert(firstrow <= lastrow);
  assert(lastrow < lp.numRow_);
  for (int row = firstrow; row <= lastrow; ++row) {
    if (XrowLower != NULL) XrowLower[row - firstrow] = lp.rowLower_[row];
    if (XrowUpper != NULL) XrowUpper[row - firstrow] = lp.rowUpper_[row];
  }
}

// Get a single coefficient from the matrix
void getLpMatrixCoefficient(const HighsLp& lp, int row, int col, double *val) {
  assert(row >= 0 && row < lp.numRow_);
  assert(col >= 0 && col < lp.numCol_);
#ifdef HiGHSDEV
  printf("Called model.util_getCoeff(row=%d, col=%d)\n", row, col);
#endif
  //  printf("Called model.util_getCoeff(row=%d, col=%d)\n", row, col);

  int get_el = -1;
  for (int el = lp.Astart_[col]; el < lp.Astart_[col + 1]; el++) {
    //  printf("Column %4d: Element %4d is row %4d. Is it %4d?\n", col, el,
    //  lp.Aindex_[el], row);
    if (lp.Aindex_[el] == row) {
      get_el = el;
      break;
    }
  }
  if (get_el < 0) {
    //  printf("model.util_getCoeff: Cannot find row %d in column %d\n", row, col);
    *val = 0;
  } else {
    //  printf("model.util_getCoeff: Found row %d in column %d as element %d:
    //  value %g\n", row, col, get_el, lp.Avalue_[get_el]);
    *val = lp.Avalue_[get_el];
  }
}

// Methods for reporting an LP, including its row and column data and matrix
//
// Report the whole LP
void reportLp(const HighsLp &lp) {
  reportLpBrief(lp);
  reportLpColVec(lp);
  reportLpRowVec(lp);
  reportLpColMtx(lp);
}

// Report the LP briefly
void reportLpBrief(const HighsLp &lp) {
  reportLpDimensions(lp);
  reportLpObjSense(lp);
}

// Report the LP dimensions
void reportLpDimensions(const HighsLp &lp) {
  HighsPrintMessage(ML_MINIMAL,
                    "LP has %d columns, %d rows and %d nonzeros\n",
                    lp.numCol_, lp.numRow_, lp.Astart_[lp.numCol_]);
}

// Report the LP objective sense
void reportLpObjSense(const HighsLp &lp) {
  if (lp.sense_ == OBJSENSE_MINIMIZE)
    HighsPrintMessage(ML_MINIMAL, "Objective sense is minimize\n");
  else if (lp.sense_ == OBJSENSE_MAXIMIZE)
    HighsPrintMessage(ML_MINIMAL, "Objective sense is maximize\n");
  else
    HighsPrintMessage(ML_MINIMAL,
                      "Objective sense is ill-defined as %d\n", lp.sense_);
}

// Report the vectors of LP column data
void reportLpColVec(const HighsLp &lp) {
  if (lp.numCol_ <= 0) return;
  HighsPrintMessage(ML_VERBOSE,
                    "  Column        Lower        Upper         Cost\n");
  for (int iCol = 0; iCol < lp.numCol_; iCol++) {
    HighsPrintMessage(ML_VERBOSE, "%8d %12g %12g %12g\n", iCol,
                      lp.colLower_[iCol], lp.colUpper_[iCol], lp.colCost_[iCol]);
  }
}

// Report the vectors of LP row data
void reportLpRowVec(const HighsLp &lp) {
  if (lp.numRow_ <= 0) return;
  HighsPrintMessage(ML_VERBOSE,
                    "     Row        Lower        Upper\n");
  for (int iRow = 0; iRow < lp.numRow_; iRow++) {
    HighsPrintMessage(ML_VERBOSE, "%8d %12g %12g\n", iRow,
                      lp.rowLower_[iRow], lp.rowUpper_[iRow]);
  }
}

// Report the LP column-wise matrix
void reportLpColMtx(const HighsLp &lp) {
  if (lp.numCol_ <= 0) return;
  HighsPrintMessage(ML_VERBOSE,
                    "Column Index              Value\n");
  for (int iCol = 0; iCol < lp.numCol_; iCol++) {
    HighsPrintMessage(ML_VERBOSE, "    %8d Start   %10d\n", iCol,
                      lp.Astart_[iCol]);
    for (int el = lp.Astart_[iCol]; el < lp.Astart_[iCol + 1]; el++) {
      HighsPrintMessage(ML_VERBOSE, "          %8d %12g\n",
                        lp.Aindex_[el], lp.Avalue_[el]);
    }
  }
  HighsPrintMessage(ML_VERBOSE, "             Start   %10d\n",
                    lp.Astart_[lp.numCol_]);
}

/*
void reportLpSolution(HighsModelObject &highs_model) {
  HighsLp lp = highs_model.simplex_lp_;
  reportLpBrief(lp);
  //  simplex_interface.report_simplex_solution_status();
  assert(lp.numCol_ > 0);
  assert(lp.numRow_ > 0);
  vector<double> colPrimal(lp.numCol_);
  vector<double> colDual(lp.numCol_);
  vector<int> colStatus(lp.numCol_);
  vector<double> rowPrimal(lp.numRow_);
  vector<double> rowDual(lp.numRow_);
  vector<int> rowStatus(lp.numRow_);
  //  util_getPrimalDualValues(colPrimal, colDual, rowPrimal, rowDual);
  //  if (util_convertWorkingToBaseStat(&colStatus[0], &rowStatus[0])) return;
  //  util_reportColVecSol(lp.numCol_, lp.colCost_, lp.colLower_, lp.colUpper_, colPrimal, colDual, colStatus);
  //  util_reportRowVecSol(lp.numRow_, lp.rowLower_, lp.rowUpper_, rowPrimal, rowDual, rowStatus);
}
*/



#ifdef HiGHSDEV
void util_analyseLp(const HighsLp &lp, const char *message) {
  printf("\n%s model data: Analysis\n", message);
  util_analyseVectorValues("Column costs", lp.numCol_, lp.colCost_, false);
  util_analyseVectorValues("Column lower bounds", lp.numCol_, lp.colLower_, false);
  util_analyseVectorValues("Column upper bounds", lp.numCol_, lp.colUpper_, false);
  util_analyseVectorValues("Row lower bounds", lp.numRow_, lp.rowLower_, false);
  util_analyseVectorValues("Row upper bounds", lp.numRow_, lp.rowUpper_, false);
  util_analyseVectorValues("Matrix sparsity", lp.Astart_[lp.numCol_], lp.Avalue_, true);
  util_analyseMatrixSparsity("Constraint matrix", lp.numCol_, lp.numRow_, lp.Astart_, lp.Aindex_);
  util_analyseModelBounds("Column", lp.numCol_, lp.colLower_, lp.colUpper_);
  util_analyseModelBounds("Row", lp.numRow_, lp.rowLower_, lp.rowUpper_);
}
#endif

