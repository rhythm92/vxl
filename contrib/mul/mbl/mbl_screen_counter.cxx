//:
// \file
// \brief Printing dots to the screen to indicate events
// \author Ian Scott

#include <mbl/mbl_screen_counter.h>
#include <vcl_ostream.h>

mbl_screen_counter::mbl_screen_counter(vcl_ostream & os):
count_(0), symbol_('a'), skip_(1), os_(os)
{
}


//: Mark event
// This is a postfix increment operator
void mbl_screen_counter::operator++ (int)
{
  count_++;
  if (count_ % skip_ == 0) os_ << symbol_ << vcl_flush;

  if (count_ / skip_ == 100)
  {
    skip_ *= 10;
    symbol_++;
    os_ << "\nReached " << count_ << ". Each '" << symbol_ << "' = " << skip_ << ":";
  }
}
