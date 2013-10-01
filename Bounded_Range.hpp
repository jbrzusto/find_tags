#ifndef BOUNDED_RANGE_HPP
#define BOUNDED_RANGE_HPP

template < class VALTYPE > class  Bounded_Range {

  // class template for bounded ranges
  // VALTYPE must be an ordered class closed under substraction

private:
  VALTYPE low;
  VALTYPE high;
  VALTYPE width;  
  bool have_bounds;

  void validate () {
    // ensure that the invariant is satisfied

    // Note: if have_bounds is false or high < low, then the interval
    // is unbounded (bi-infinite, in the case of numeric VALTYPE)

    if (have_bounds) {
      if (high - low > width)
	throw std::runtime_error("Bounded_Range: must have high - low > width");
    }
  };

public:

  Bounded_Range () :
    have_bounds(false)
  {};
    
  Bounded_Range (VALTYPE width) :
    width(width),
    have_bounds(false)
  {
  };

  Bounded_Range (VALTYPE width, VALTYPE lowhigh) :
    low(lowhigh),
    high(lowhigh),
    width(width),
    have_bounds(true)
  {
    validate();
  };

  Bounded_Range (VALTYPE width, VALTYPE low, VALTYPE high) :
    low(low),
    high(high),
    width(width),
    have_bounds(true)
  {
    validate();
  };
 
  bool is_in (VALTYPE p) {
    return (! have_bounds) || p >= low && p <= high;
  };

  bool is_compatible (VALTYPE p) {
    return (! have_bounds) || (high - p <= width && p - low <= width);
  };

  void clear_bounds() {
    have_bounds = false;
  };

  bool extend_by (VALTYPE p) {
    // try extend the interval to include p
    if (have_bounds && high >= low) {
      if (p > high) {
	if (p - low <= width) {
	  high = p;
	  return true;
	} else {
	  return false;
	}
      }
      if (p < low) {
	if (high - p <= width) {
	  low = p;
	  return true;
	} else {
	  return false;
	}
      }
      return true;
    } else {
      low = high = p;
      width = high - low;
      return true;
    }
  };
};

#endif // BOUNDED_RANGE_HPP
