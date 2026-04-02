<?php
/**
 * @file
 * Entity classes for DropTime.
 *
 * Note: DropProject entity class lives in dropcrm.entity.inc.
 */

// ---------------------------------------------------------------------------
// DropTimeEntry
// ---------------------------------------------------------------------------

/**
 * Entity class for a time log entry.
 */
class DropTimeEntry extends Entity {
  public $eid;
  public $pid        = 0;
  public $uid        = 0;
  public $entry_date = '';
  public $start_time = '';
  public $end_time   = '';
  public $duration   = 0;
  public $notes      = '';
  public $billable   = 1;
  public $created    = 0;
  public $changed    = 0;

  /** {@inheritdoc} */
  public function id() {
    return $this->eid;
  }

  /** {@inheritdoc} */
  public function entityType() {
    return 'drop_time_entry';
  }

  /** {@inheritdoc} */
  protected function defaultLabel() {
    return $this->entry_date . ' ' . $this->start_time;
  }

  /** {@inheritdoc} */
  public function uri() {
    return array('path' => 'drop/time', 'options' => array());
  }

  /** {@inheritdoc} */
  protected function defaultUri() {
    return $this->uri();
  }
}

/**
 * Controller for DropTimeEntry entities.
 */
class DropTimeEntryController extends EntityDatabaseStorageController {

  /** {@inheritdoc} */
  public function save($entity) {
    global $user;
    if (empty($entity->created)) {
      $entity->created = REQUEST_TIME;
    }
    $entity->changed = REQUEST_TIME;
    if (empty($entity->uid)) {
      $entity->uid = $user->uid;
    }
    // Compute duration in minutes from start and end times.
    if (!empty($entity->start_time) && !empty($entity->end_time)) {
      list($sh, $sm) = explode(':', $entity->start_time);
      list($eh, $em) = explode(':', $entity->end_time);
      $entity->duration = max(0, ($eh * 60 + $em) - ($sh * 60 + $sm));
    }
    return parent::save($entity);
  }
}
