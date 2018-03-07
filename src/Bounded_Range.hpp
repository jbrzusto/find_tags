#ifndef BOUNDED_RANGE_HPP
#define BOUNDED_RANGE_HPP

template < class VALTYPE > class  Bounded_Range {

  // class template for bounded ranges VALTYPE must be an ordered
  // class closed under subtraction, and must have addition and
  // division operators, as these are used to obtain the centre of the
  // range.

  friend class Tag_Foray;

private:
  VALTYPE low;
  VALTYPE high;
  VALTYPE width;  // if negative, have_bounds is never true, is_in and
                  // is_compatible are always true,
                  // and extend_by is a no-op; this makes the bounded range infinite.
  bool have_bounds;

  void validate () {
    // ensure that the invariant is satisfied

    // Note: if widthif have_bounds is false or high < low, then the interval
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
    have_bounds(width >= 0)
  {
    validate();
  };

  Bounded_Range (VALTYPE width, VALTYPE low, VALTYPE high) :
    low(low),
    high(high),
    width(width),
    have_bounds(width >= 0)
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

  void pin_to_centre() {
    // reset both bounds to centre of current interval.
    // This allows the interval to grow in either direction
    // away from that value.
    if (! have_bounds) 
      return;
    VALTYPE c = (high + low) / 2.0;
    high = low = c;
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
      if (width >= 0) {
        low = high = p;
        have_bounds = true;
      }
      return true;
    }
  };

  template < class Archive >
  void serialize(Archive & ar, const unsigned int version) {
    ar & BOOST_SERIALIZATION_NVP( low );
    ar & BOOST_SERIALIZATION_NVP( high );
    ar & BOOST_SERIALIZATION_NVP( width );
    ar & BOOST_SERIALIZATION_NVP( have_bounds );
  };

};
  
#endif // BOUNDED_RANGE_HPP
