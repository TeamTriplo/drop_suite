<?php
/**
 * @file
 * Contains the Clients report summary area handler.
 */

/**
 * "24 clients — 5 not updated in the last 30 days."
 *
 * Counts the rows on the current page, so it is meant for a view without a
 * pager. The stale count uses the view's "Last updated" field.
 *
 * @ingroup views_area_handlers
 */
class drop_crm_handler_area_client_summary extends views_handler_area {

  /**
   * {@inheritdoc}
   */
  function render($empty = FALSE) {
    if ($empty) {
      return '';
    }
    $total = count($this->view->result);
    $output = format_plural($total, '1 client', '@count clients');

    foreach ($this->view->field as $field) {
      if ($field instanceof drop_crm_handler_field_last_updated) {
        $stale = 0;
        foreach ($this->view->result as $row) {
          if ($field->is_stale($row)) {
            $stale++;
          }
        }
        if ($stale) {
          $output .= ' — ' . t('@count not updated in the last @days days.', array(
            '@count' => $stale,
            '@days' => drop_crm_client_stale_days(),
          ));
        }
        break;
      }
    }
    return '<p class="drop_crm-report-count">' . $output . '</p>';
  }

}
