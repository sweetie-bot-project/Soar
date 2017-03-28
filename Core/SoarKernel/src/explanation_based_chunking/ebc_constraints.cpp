/*
 * variablization_manager_constraints.cpp
 *
 *  Created on: Jul 25, 2013
 *      Author: mazzin
 */

#include "ebc.h"
#include "ebc_identity_set.h"

#include "agent.h"
#include "condition.h"
#include "dprint.h"
#include "explanation_memory.h"
#include "instantiation.h"
#include "print.h"
#include "test.h"
#include "working_memory.h"

void Explanation_Based_Chunker::clear_cached_constraints()
{
    for (constraint_list::iterator it = constraints->begin(); it != constraints->end(); ++it)
    {
        /* We intentionally used the tests in the conditions backtraced through instead of copying
         * them, so we don't need to deallocate the tests in the constraint. We just delete the
         * constraint struct that contains the two pointers.*/
        thisAgent->memoryManager->free_with_pool(MP_constraints, *it);
    }
    constraints->clear();
}

void Explanation_Based_Chunker::cache_constraints_in_test(test t)
{
    test ctest;
    constraint* new_constraint = NULL;

    for (cons* c = t->data.conjunct_list; c != NIL; c = c->rest)
    {
        ctest = static_cast<test>(c->first);
        if (test_can_be_transitive_constraint(ctest))
        {

            thisAgent->memoryManager->allocate_with_pool(MP_constraints, &new_constraint);
            new_constraint->eq_test = t->eq_test;
            new_constraint->constraint_test = ctest;
            dprint(DT_CONSTRAINTS, "Caching constraints on %t [%g]: %t [%g]\n", new_constraint->eq_test, new_constraint->eq_test, new_constraint->constraint_test, new_constraint->constraint_test);
            constraints->push_back(new_constraint);
            #ifdef EBC_DETAILED_STATISTICS
                thisAgent->explanationMemory->increment_stat_constraints_collected();
            #endif
        }
    }
}

void Explanation_Based_Chunker::cache_constraints_in_cond(condition* c)
{
//    dprint(DT_CONSTRAINTS, "Caching relational constraints in condition: %l\n", c);
    if (c->data.tests.id_test->type == CONJUNCTIVE_TEST) cache_constraints_in_test(c->data.tests.id_test);
    if (c->data.tests.attr_test->type == CONJUNCTIVE_TEST) cache_constraints_in_test(c->data.tests.attr_test);
    if (c->data.tests.value_test->type == CONJUNCTIVE_TEST) cache_constraints_in_test(c->data.tests.value_test);
}

void Explanation_Based_Chunker::invert_relational_test(test* pEq_test, test* pRelational_test)
{
    TestType tt = (*pRelational_test)->type;
    if (tt == NOT_EQUAL_TEST)
    {
        (*pEq_test)->type = NOT_EQUAL_TEST;
    }
    else if (tt == LESS_TEST)
    {
        (*pEq_test)->type = GREATER_TEST;
    }
    else if (tt == GREATER_TEST)
    {
        (*pEq_test)->type = LESS_TEST;
    }
    else if (tt == LESS_OR_EQUAL_TEST)
    {
        (*pEq_test)->type = GREATER_OR_EQUAL_TEST;
    }
    else if (tt == GREATER_OR_EQUAL_TEST)
    {
        (*pEq_test)->type = LESS_OR_EQUAL_TEST;
    }
    else if (tt == SAME_TYPE_TEST)
    {
        (*pEq_test)->type = SAME_TYPE_TEST;
    }
    (*pRelational_test)->type = EQUALITY_TEST;

    test temp = *pEq_test;
    (*pEq_test) = (*pRelational_test);
    (*pRelational_test) = temp;

}

void Explanation_Based_Chunker::attach_relational_test(test pRelational_test, condition* pCond, WME_Field pField)
{
    dprint(DT_CONSTRAINTS, "Attaching transitive constraint %t %g to field %d of %l.\n", pRelational_test, pRelational_test, static_cast<int64_t>(pField), pCond);
    if (pField == VALUE_ELEMENT)
    {
        add_test(thisAgent, &(pCond->data.tests.value_test), pRelational_test, true);
    } else if (pField == ATTR_ELEMENT)
    {
        add_test(thisAgent, &(pCond->data.tests.attr_test), pRelational_test, true);
    } else
    {
        add_test(thisAgent, &(pCond->data.tests.id_test), pRelational_test, true);
    }
    #ifdef EBC_DETAILED_STATISTICS
        thisAgent->explanationMemory->increment_stat_constraints_attached();
    #endif
}

void Explanation_Based_Chunker::add_additional_constraints()
{
    constraint* lConstraint = NULL;
    test eq_copy = NULL, constraint_test = NULL;

    dprint_header(DT_CONSTRAINTS, PrintBefore, "Adding %u transitive constraints from non-operational conditions...\n", static_cast<uint64_t>(constraints->size()));

    for (constraint_list::iterator iter = constraints->begin(); iter != constraints->end();)
    {
        lConstraint = *iter;
        condition* lOperationalCond = lConstraint->eq_test->identity_set ? lConstraint->eq_test->identity_set->get_operational_cond() : NULL;
        condition* lOperationalConstraintCond = lConstraint->constraint_test->identity_set ? lConstraint->constraint_test->identity_set->get_operational_cond() : NULL;
        dprint(DT_CONSTRAINTS, "Attempting to add constraint %t %g to %t %g: ", lConstraint->constraint_test, lConstraint->constraint_test, lConstraint->eq_test, lConstraint->eq_test);

        if (lOperationalCond && !lConstraint->eq_test->identity_set->literalized())
        {
            constraint_test = copy_test(thisAgent, lConstraint->constraint_test, true);
            attach_relational_test(constraint_test, lOperationalCond, lConstraint->eq_test->identity_set->get_operational_field());
            dprint(DT_CONSTRAINTS, "...constraint added.  Condition is now %l\n", lOperationalCond);
        }
        else if (lOperationalConstraintCond && !lConstraint->constraint_test->identity_set->literalized())
        {
            eq_copy = copy_test(thisAgent, lConstraint->eq_test, true);
            constraint_test = copy_test(thisAgent, lConstraint->constraint_test, true);
            invert_relational_test(&eq_copy, &constraint_test);
            attach_relational_test(constraint_test, lOperationalConstraintCond, lConstraint->constraint_test->identity_set->get_operational_field());
            deallocate_test(thisAgent, eq_copy);
            dprint(DT_CONSTRAINTS, "...complement of constraint added.  Condition is now %l\n", lOperationalConstraintCond);
        } else {
            dprint(DT_CONSTRAINTS, "...did not add constraint:\n    eq_test: %t %g, literalized = %s\n    reltest: %t %g, literalized = %s\n",
                lConstraint->eq_test, lConstraint->eq_test, (lConstraint->eq_test->identity_set && lConstraint->eq_test->identity_set->literalized()) ? "true" : "false",
                    lConstraint->constraint_test, lConstraint->constraint_test, (lConstraint->constraint_test->identity_set && lConstraint->constraint_test->identity_set->literalized()) ? "true" : "false");
        }
        ++iter;
    }
    clear_cached_constraints();

    dprint_header(DT_CONSTRAINTS, PrintAfter, "Done adding transitive constraints.\n");
}
