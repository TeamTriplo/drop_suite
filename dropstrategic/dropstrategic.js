(function ($) {
  'use strict';

  /**
   * Key result checkbox toggle via AJAX.
   */
  Backdrop.behaviors.dropstrategicKrToggle = {
    attach: function (context, settings) {
      $('.dropstrategic-kr-check', context).once('kr-toggle').on('change', function () {
        var $check = $(this);
        var url    = $check.data('url');
        var $item  = $check.closest('.dropstrategic-kr-item');

        $.getJSON(url, function (data) {
          if (data.error) { return; }
          if (data.done) {
            $item.addClass('dropstrategic-kr-item--done');
            $check.prop('checked', true);
          }
          else {
            $item.removeClass('dropstrategic-kr-item--done');
            $check.prop('checked', false);
          }
          if (typeof data.progress !== 'undefined') {
            var pct = data.progress;
            $('.dropstrategic-progress-fill').css('width', pct + '%');
            $('.dropstrategic-progress-label').text(pct + '%');
          }
        });
      });
    }
  };

}(jQuery));
