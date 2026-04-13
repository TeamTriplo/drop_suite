<?php
/**
 * @file
 * Entity class and controller for DropUpdates drop_update entities.
 */

/**
 * Entity class for a team update.
 */
class DropUpdate extends Entity {

  /** @var int Primary key. */
  public $uid_post;

  /** @var int Author uid. */
  public $uid = 0;

  /** @var string Body text. */
  public $body = '';

  /** @var string Text format machine name. */
  public $body_format = 'filtered_html';

  /** @var int Created timestamp. */
  public $created = 0;

  /** @var int Last-modified timestamp. */
  public $changed = 0;

  /**
   * {@inheritdoc}
   */
  public function id() {
    return $this->uid_post;
  }

  /**
   * {@inheritdoc}
   */
  public function entityType() {
    return 'drop_update';
  }

  /**
   * {@inheritdoc}
   */
  protected function defaultLabel() {
    // Updates don't have titles; label with author + date for admin UIs.
    $author = user_load($this->uid);
    $name = $author ? format_username($author) : t('Unknown');
    return t('@name · @date', array(
      '@name' => $name,
      '@date' => $this->created ? format_date($this->created, 'short') : '',
    ));
  }

  /**
   * {@inheritdoc}
   */
  public function uri() {
    return $this->defaultUri();
  }

  /**
   * {@inheritdoc}
   */
  protected function defaultUri() {
    return array('path' => 'drop/updates', 'options' => array('fragment' => 'u-' . $this->uid_post));
  }

}

/**
 * Controller class for drop_update entities.
 */
class DropUpdateController extends EntityDatabaseStorageController {

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
