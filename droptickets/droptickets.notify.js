/**
 * @file
 * Polls for unread notification count and updates the bell badge + tab title.
 */

(function ($) {
  'use strict';

  var POLL_INTERVAL = 180000; // 3 minutes.
  var originalTitle = document.title;
  var timer;

  /**
   * Updates the bell badge and browser tab title with the current count.
   */
  function updateBell(count) {
    var $bell = $('.drop-nav-bell');
    if (!$bell.length) {
      return;
    }

    var $badge = $bell.find('.drop-nav-bell-count');

    if (count > 0) {
      if ($badge.length) {
        $badge.text(count);
      }
      else {
        $bell.append(' <span class="drop-nav-bell-count">' + count + '</span>');
      }
      $bell.addClass('drop-nav-bell--active');
      document.title = '(' + count + ') ' + originalTitle;
    }
    else {
      $badge.remove();
      $bell.removeClass('drop-nav-bell--active');
      document.title = originalTitle;
    }
  }

  /**
   * Fetches the current unread count from the server.
   */
  function poll() {
    var url = Backdrop.settings.droptickets && Backdrop.settings.droptickets.notifyUrl;
    if (!url) {
      return;
    }
    $.getJSON(url, function (data) {
      if (typeof data.count !== 'undefined') {
        updateBell(data.count);
      }
    });
  }

  Backdrop.behaviors.dropticketsNotifyPoll = {
    attach: function (context) {
      // Only run once on full page load, not on AJAX-attached content.
      if (context !== document) {
        return;
      }
      if (timer) {
        return;
      }

      // Capture the original title before any modification.
      originalTitle = document.title.replace(/^\(\d+\)\s*/, '');

      timer = setInterval(poll, POLL_INTERVAL);
    }
  };

}(jQuery));
