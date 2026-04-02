/**
 * @file
 * DropTime — live elapsed time display and AJAX timer controls.
 */
(function ($) {

  /**
   * Live elapsed time ticker.
   *
   * Reads the start timestamp from the data-timer-start attribute on the
   * .droptime-timer-running element so this works both on page load and after
   * an AJAX start (no Backdrop.settings dependency).
   */
  Backdrop.behaviors.droptimeTimer = {
    attach: function (context, settings) {
      $('.droptime-timer-running', context).once('droptime-ticker').each(function () {
        var $banner  = $(this);
        var startTs  = parseInt($banner.data('timerStart'), 10) * 1000;
        var $elapsed = $banner.find('.droptime-elapsed');

        function update() {
          var secs  = Math.floor((Date.now() - startTs) / 1000);
          var h     = Math.floor(secs / 3600);
          var m     = Math.floor((secs % 3600) / 60);
          var s     = secs % 60;
          var parts = [];
          if (h) { parts.push(h + 'h'); }
          if (m || h) { parts.push(m + 'm'); }
          parts.push(s + 's');
          $elapsed.text('(' + parts.join(' ') + ')');
        }

        update();
        setInterval(update, 1000);
      });
    }
  };

  /**
   * AJAX timer start — fires the start endpoint without a page reload.
   *
   * On success, inserts the server-rendered banner into #droptime-banner-area
   * and hides the start link. Falls back to a full page reload on error.
   */
  Backdrop.behaviors.droptimeTimerControls = {
    attach: function (context, settings) {
      $('.js-droptime-start', context).once('droptime-start').on('click', function (e) {
        e.preventDefault();
        var url = $(this).data('url');

        $.post(url, {_dt_ajax: '1'}, function (data) {
          if (!data.ts) {
            window.location.reload();
            return;
          }
          var $bannerArea = $('#droptime-banner-area');
          $bannerArea.html(data.bannerHtml);
          $('#droptime-nav-timer-action').hide();
          Backdrop.attachBehaviors($bannerArea[0]);
        }, 'json').fail(function () {
          window.location.reload();
        });
      });
    }
  };

  /**
   * Time-log reminder banner dismiss.
   *
   * Sets a cookie with the current Unix timestamp so the server can suppress
   * the banner for the next hour without a flash on page load.
   */
  Backdrop.behaviors.droptimeReminder = {
    attach: function (context, settings) {
      $('.drop-time-reminder-dismiss', context).once('droptime-reminder').on('click', function () {
        // Set a 1-hour cookie keyed by the current timestamp.
        var expires = new Date(Date.now() + 60 * 60 * 1000).toUTCString();
        document.cookie = 'drop_time_reminder_dismissed=' + Math.floor(Date.now() / 1000)
          + '; expires=' + expires + '; path=/';
        $('#drop-time-reminder').slideUp(200, function () { $(this).remove(); });
      });
    }
  };

  /**
   * Datepicker for the edit entry form.
   */
  Backdrop.behaviors.droptimeDatepicker = {
    attach: function (context, settings) {
      $('.droptime-datepicker', context).once('droptime-datepicker').datepicker({
        dateFormat: 'yy-mm-dd',
        changeMonth: true,
        changeYear: true
      });
    }
  };

}(jQuery));
