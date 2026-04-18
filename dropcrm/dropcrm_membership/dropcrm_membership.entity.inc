<?php
/**
 * @file
 * Entity class and controller definitions for DropCRM Membership.
 *
 * Classes extend Backdrop core's Entity. Controllers extend EntityPlusController
 * from the entity_plus module — same pattern as DropCRM core entities.
 */

// ---------------------------------------------------------------------------
// CrmMembership
// ---------------------------------------------------------------------------

/**
 * Entity class for memberships.
 */
class CrmMembership extends Entity {

  public function id() {
    return isset($this->mid) ? $this->mid : NULL;
  }

  public function entityType() {
    return 'crm_membership';
  }

  public function bundle() {
    // Type is a base-table varchar, not a bundle. Return the entity type so
    // field API treats all memberships as sharing one bundle.
    return 'crm_membership';
  }

  /**
   * Returns a human-readable label.
   *
   * Prefers member_id when set; falls back to the contact name + type.
   */
  public function label() {
    if (!empty($this->member_id)) {
      return $this->member_id;
    }
    if (!empty($this->cid)) {
      $contact = entity_load('crm_contact', $this->cid);
      if ($contact) {
        $types = _dropcrm_membership_types();
        $type_label = isset($types[$this->type]['label']) ? $types[$this->type]['label'] : $this->type;
        return $contact->label() . ' (' . $type_label . ')';
      }
    }
    return t('Membership !mid', array('!mid' => $this->id()));
  }

  public function uri() {
    return array('path' => 'drop/crm/memberships/' . $this->id(), 'options' => array());
  }

  /**
   * Returns the earliest term's start_date as YYYY-MM-DD, or NULL.
   */
  public function joinedDate() {
    $date = db_query('SELECT MIN(start_date) FROM {crm_membership_term} WHERE mid = :mid', array(':mid' => $this->mid))->fetchField();
    if (empty($date) || $date === '0000-00-00') {
      return NULL;
    }
    return $date;
  }

  /**
   * Returns the latest term's end_date as YYYY-MM-DD, or NULL for lifetime.
   */
  public function currentEndDate() {
    $term = _dropcrm_membership_latest_term($this->mid);
    return $term && !empty($term->end_date) ? $term->end_date : NULL;
  }

}

/**
 * Controller for CrmMembership.
 */
class CrmMembershipController extends EntityPlusController {
}

// ---------------------------------------------------------------------------
// CrmMembershipTerm
// ---------------------------------------------------------------------------

/**
 * Entity class for membership terms.
 */
class CrmMembershipTerm extends Entity {

  public function id() {
    return isset($this->tid) ? $this->tid : NULL;
  }

  public function entityType() {
    return 'crm_membership_term';
  }

  public function bundle() {
    return 'crm_membership_term';
  }

  public function label() {
    $start = !empty($this->start_date) ? $this->start_date : '';
    $end   = !empty($this->end_date) ? $this->end_date : t('lifetime');
    return t('Term !start – !end', array('!start' => $start, '!end' => $end));
  }

  public function uri() {
    // Terms don't have a standalone view page. Point to the parent membership.
    $mid = !empty($this->mid) ? $this->mid : 0;
    return array('path' => 'drop/crm/memberships/' . $mid, 'options' => array());
  }

}

/**
 * Controller for CrmMembershipTerm.
 */
class CrmMembershipTermController extends EntityPlusController {
}

// ---------------------------------------------------------------------------
// CrmMembershipPayment
// ---------------------------------------------------------------------------

/**
 * Entity class for membership payments.
 */
class CrmMembershipPayment extends Entity {

  public function id() {
    return isset($this->pid) ? $this->pid : NULL;
  }

  public function entityType() {
    return 'crm_membership_payment';
  }

  public function bundle() {
    return 'crm_membership_payment';
  }

  public function label() {
    $amount = number_format((float) (isset($this->amount) ? $this->amount : 0), 2);
    $date = !empty($this->paid_date) ? $this->paid_date : '';
    return t('$!amount on !date', array('!amount' => $amount, '!date' => $date));
  }

  public function uri() {
    $mid = !empty($this->mid) ? $this->mid : 0;
    return array('path' => 'drop/crm/memberships/' . $mid, 'options' => array());
  }

}

/**
 * Controller for CrmMembershipPayment.
 */
class CrmMembershipPaymentController extends EntityPlusController {
}
