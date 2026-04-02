<?php
/**
 * @file
 * Entity class and controller for DropTasks task entities.
 */

/**
 * Entity class for a DropTasks task.
 */
class DroptTask extends Entity {

  /** @var int Task ID. */
  public $tid;

  /** @var string Task title. */
  public $title = '';

  /** @var string Status: 'pending' or 'complete'. */
  public $status = 'pending';

  /** @var int Priority: 0=none, 1=low, 2=normal, 3=high, 4=critical (matches drop_base_priority_labels()). */
  public $priority = 2;

  /** @var int|null Unix timestamp due date, or NULL. */
  public $due_date = NULL;

  /** @var string Optional notes. */
  public $notes = '';

  /** @var int Owner's uid. */
  public $uid = 0;

  /** @var int Created timestamp. */
  public $created = 0;

  /** @var int Changed timestamp. */
  public $changed = 0;

  /** @var int|null Optional CRM contact ID linked to this task. */
  public $contact_id = NULL;

  /** @var int|null Optional CRM activity ID that spawned this task. */
  public $source_activity_id = NULL;

  /** @var int Completion mode: 0=shared, 1=individual per-assignee. */
  public $completion_mode = 0;

  /** @var string Recurrence: '', 'daily', 'weekly', 'monthly'. */
  public $recurrence = '';

  /** @var string Comma-separated day numbers for weekly recurrence (e.g. '2,4'). */
  public $recurrence_days = '';

  /** @var string Monthly type: 'day_of_month' or 'nth_weekday'. */
  public $recurrence_month_type = 'day_of_month';

  /** @var int Which week of month (1–4 or -1 for last) for nth_weekday. */
  public $recurrence_month_week = 1;

  /** @var int Day of week (0=Sun…6=Sat) for nth_weekday monthly recurrence. */
  public $recurrence_month_weekday = 1;

  /**
   * {@inheritdoc}
   */
  public function id() {
    return $this->tid;
  }

  /**
   * {@inheritdoc}
   */
  public function entityType() {
    return 'drop_task';
  }

  /**
   * {@inheritdoc}
   */
  public function uri() {
    return array('path' => 'drop/tasks/' . $this->tid, 'options' => array());
  }

  /**
   * {@inheritdoc}
   */
  protected function defaultLabel() {
    return $this->title;
  }

  /**
   * {@inheritdoc}
   */
  protected function defaultUri() {
    return array('path' => 'drop/tasks/' . $this->tid, 'options' => array());
  }

  /**
   * Returns TRUE if this task is overdue.
   *
   * @return bool
   */
  public function isOverdue() {
    return $this->status === 'pending'
      && !empty($this->due_date)
      && $this->due_date < strtotime('today midnight');
  }

  /**
   * Returns TRUE if this task is due today.
   *
   * @return bool
   */
  public function isDueToday() {
    if (empty($this->due_date) || $this->status !== 'pending') {
      return FALSE;
    }
    $today_start = strtotime('today midnight');
    $today_end   = strtotime('tomorrow midnight') - 1;
    return $this->due_date >= $today_start && $this->due_date <= $today_end;
  }

}

/**
 * Controller class for DroptTask entities.
 */
class DroptTaskController extends EntityDatabaseStorageController {

  /**
   * {@inheritdoc}
   */
  public function save($entity) {
    $now = REQUEST_TIME;
    if (empty($entity->created)) {
      $entity->created = $now;
    }
    $entity->changed = $now;
    if (empty($entity->uid)) {
      global $user;
      $entity->uid = $user->uid;
    }
    return parent::save($entity);
  }

}
