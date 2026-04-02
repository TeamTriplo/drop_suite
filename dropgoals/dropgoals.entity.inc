<?php
/**
 * @file
 * Entity class and controller for DropGoals goal entities.
 */

/**
 * Entity class for a DropGoals goal definition.
 */
class DropGoal extends Entity {

  /** @var int Goal ID. */
  public $gid;

  /** @var string Goal label. */
  public $label = '';

  /** @var string Optional description. */
  public $description = '';

  /** @var int Whether the goal is active (1) or inactive (0). */
  public $active = 1;

  /** @var int Owner's uid. */
  public $uid = 0;

  /** @var int Created timestamp. */
  public $created = 0;

  /** @var string How often this goal repeats: daily, every_other_day, weekly, monthly, specific_days. */
  public $frequency = 'daily';

  /** @var string Comma-separated ISO day numbers (1=Mon…7=Sun) used when frequency is specific_days. */
  public $days_of_week = '';

  /** @var int Changed timestamp. */
  public $changed = 0;

  /**
   * {@inheritdoc}
   */
  public function id() {
    return $this->gid;
  }

  /**
   * {@inheritdoc}
   */
  public function entityType() {
    return 'drop_goal';
  }

  /**
   * {@inheritdoc}
   */
  protected function defaultLabel() {
    return $this->label;
  }

  /**
   * {@inheritdoc}
   */
  public function uri() {
    return array('path' => 'drop/goals/manage/' . $this->gid, 'options' => array());
  }

  /**
   * {@inheritdoc}
   */
  protected function defaultUri() {
    return array('path' => 'drop/goals/manage/' . $this->gid, 'options' => array());
  }

}

/**
 * Controller class for DropGoal entities.
 */
class DropGoalController extends EntityDatabaseStorageController {

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
