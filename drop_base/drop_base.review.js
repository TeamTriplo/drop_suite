(function ($) {
  'use strict';

  /**
   * Review checklist toggle buttons.
   *
   * Works with any module that renders .drop-review-btn elements with a
   * data-url attribute pointing to an AJAX toggle endpoint returning
   * {id, reviewed}.
   */
  Backdrop.behaviors.dropBaseReview = {
    attach: function (context) {
      $('.drop-review-btn', context).once('drop-review').on('click', function () {
        var $btn = $(this);
        var $row = $btn.closest('tr');
        var url  = $btn.data('url');

        $btn.prop('disabled', true);

        $.post(url, function (data) {
          if (data.reviewed) {
            $btn.addClass('drop-review-btn--done')
                .attr('title', Backdrop.t('Reviewed \u2014 click to unmark'));
            $row.addClass('drop-review-row--done');
          }
          else {
            $btn.removeClass('drop-review-btn--done')
                .attr('title', Backdrop.t('Mark as reviewed'));
            $row.removeClass('drop-review-row--done');
          }
        }, 'json').always(function () {
          $btn.prop('disabled', false);
        });
      });
    }
  };

}(jQuery));
