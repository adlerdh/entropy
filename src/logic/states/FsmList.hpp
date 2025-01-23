#ifndef FSMLIST_HPP
#define FSMLIST_HPP

#include "logic/states/AnnotationStateMachine.h"

#include <tinyfsm.hpp>


namespace state
{

/**
 * @brief The list of all state machines. So far we have only one.
 * If we have multiple state machines, then the FsmList can be used
 * to dispatch events to them all, e.g.:
 * using fsm_list = tinyfsm::FsmList< AnnotationStateMachine >;
 */
using fsm_list = AnnotationStateMachine;


/**
 * @brief Dispatch event to the state machine(s)
 */
template<typename E>
void send_event( const E& event )
{
    fsm_list::template dispatch<E>( event );
}

} // namespace state

#endif // FSMLIST_HPP
