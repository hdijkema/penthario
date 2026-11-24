#pragma once
#include <FL/Fl.H>
#include <FL/Fl_Tree.H>

namespace pgjr
{
// Fl_Tree's selection callback is left-button oriented. Forward a right press once,
// retaining the actual item as callback context without triggering lazy expansion.
class ContextTree : public Fl_Tree
{
  public:
    using Fl_Tree::Fl_Tree;
    int handle(int event) override
    {
        if (event == FL_PUSH && Fl::event_button() == FL_RIGHT_MOUSE)
        {
            if (auto *item = find_clicked(1))
            {
                select_only(item, 0);
                callback_item(item);
                callback_reason(FL_TREE_REASON_NONE);
                do_callback();
            }
            return 1;
        }
        if (event == FL_RELEASE && Fl::event_button() == FL_RIGHT_MOUSE)
            return 1;
        return Fl_Tree::handle(event);
    }
};
} // namespace pgjr
