<?php
/**
 * @file
 * Entity classes for DropTickets.
 */

// ---------------------------------------------------------------------------
// DropTicket
// ---------------------------------------------------------------------------

/**
 * Entity class for a ticket.
 */
class DropTicket extends Entity {
  public $tid;
  public $pid           = 0;
  public $ticket_number = 0;
  public $title         = '';
  public $body          = '';
  public $body_format   = 'filtered_html';
  public $type          = 'task';
  public $status        = 'open';
  public $priority      = 1;
  public $owner_uid     = NULL;
  public $assignee_uid  = NULL;
  public $assignee_mode = 'user';
  public $due_date      = NULL;
  public $uid           = 0;
  public $created       = 0;
  public $changed       = 0;
  public $import_id     = NULL;
  public $external_link = NULL;
  public $inbound_email = NULL;
  public $is_verified   = 1;

  /** {@inheritdoc} */
  public function id() {
    return $this->tid;
  }

  /** {@inheritdoc} */
  public function entityType() {
    return 'drop_ticket';
  }

  /** {@inheritdoc} */
  protected function defaultLabel() {
    return $this->title;
  }

  /** {@inheritdoc} */
  public function uri() {
    return array('path' => 'drop/tickets/' . $this->tid, 'options' => array());
  }

  /** {@inheritdoc} */
  protected function defaultUri() {
    return $this->uri();
  }
}

/**
 * Controller for DropTicket entities.
 */
class DropTicketController extends EntityPlusController {

  /** {@inheritdoc} */
  public function save($entity, ?DatabaseTransaction $transaction = NULL) {
    global $user;

    if (empty($entity->created)) {
      $entity->created = REQUEST_TIME;

      // Assign the next ticket number on insert.
      if (!empty($entity->pid)) {
        // Project-scoped counter stored on the project row.
        db_update('drop_project')
          ->expression('ticket_counter', 'ticket_counter + 1')
          ->condition('pid', $entity->pid)
          ->execute();

        $entity->ticket_number = (int) db_select('drop_project', 'p')
          ->fields('p', array('ticket_counter'))
          ->condition('pid', $entity->pid)
          ->execute()
          ->fetchField();
      }
      else {
        // No project assigned — use a global counter via state API.
        $next = (int) state_get('droptickets_unassigned_counter', 0) + 1;
        state_set('droptickets_unassigned_counter', $next);
        $entity->ticket_number = $next;
      }
    }

    // On update: if pid changed, assign a fresh ticket number for the new project.
    if (!empty($entity->tid) && !empty($entity->created) && !empty($entity->pid)) {
      $old_pid = (int) db_select('drop_ticket', 't')
        ->fields('t', array('pid'))
        ->condition('tid', $entity->tid)
        ->execute()
        ->fetchField();
      if ($old_pid !== (int) $entity->pid) {
        db_update('drop_project')
          ->expression('ticket_counter', 'ticket_counter + 1')
          ->condition('pid', $entity->pid)
          ->execute();
        $entity->ticket_number = (int) db_select('drop_project', 'p')
          ->fields('p', array('ticket_counter'))
          ->condition('pid', $entity->pid)
          ->execute()
          ->fetchField();
      }
    }

    $entity->changed = REQUEST_TIME;

    if (empty($entity->uid)) {
      $entity->uid = $user->uid;
    }

    return parent::save($entity);
  }
}

// ---------------------------------------------------------------------------
// DropTicketComment
// ---------------------------------------------------------------------------

/**
 * Entity class for a ticket comment.
 */
class DropTicketComment extends Entity {
  public $tcid;
  public $tid         = 0;
  public $body        = '';
  public $body_format = 'filtered_html';
  public $uid         = 0;
  public $created     = 0;
  public $changed     = 0;
  public $source      = 'comment';

  /** {@inheritdoc} */
  public function id() {
    return $this->tcid;
  }

  /** {@inheritdoc} */
  public function entityType() {
    return 'drop_ticket_comment';
  }

  /** {@inheritdoc} */
  protected function defaultLabel() {
    return t('Comment #@id', array('@id' => $this->tcid));
  }

  /** {@inheritdoc} */
  public function uri() {
    return array('path' => 'drop/tickets/' . $this->tid, 'options' => array());
  }

  /** {@inheritdoc} */
  protected function defaultUri() {
    return $this->uri();
  }
}

/**
 * Controller for DropTicketComment entities.
 */
class DropTicketCommentController extends EntityDatabaseStorageController {

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
    return parent::save($entity);
  }
}
