<?php
/**
 * @file
 * NULL-safe Views date field handler for Drop CRM Membership.
 */

/**
 * Extends views_handler_field_date to guard against NULL values.
 *
 * The built-in handler passes the field value straight to strtotime(), which
 * on PHP 8.1+ emits a deprecation notice when called with NULL. Some of our
 * columns are intentionally nullable (crm_membership_term.end_date for
 * lifetime terms, crm_membership.last_payment_date when no payments
 * exist yet). Returning an empty string when the value is NULL or ''
 * avoids the deprecation without patching core.
 */
class DropcrmMembershipHandlerFieldDateSafe extends views_handler_field_date {

  function render($values) {
    $value = $this->get_value($values);
    if ($value === NULL || $value === '' || $value === '0000-00-00') {
      return '';
    }
    return parent::render($values);
  }

}
