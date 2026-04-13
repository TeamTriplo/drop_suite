(function ($, Backdrop) {
  'use strict';

  /**
   * IntersectionObserver-based read tracking.
   *
   * An update card is marked "seen" after it has been at least 50% visible
   * in the viewport for 2 seconds. Once marked, the observer stops watching
   * that card.
   */
  Backdrop.behaviors.dropupdatesReadTracking = {
    attach: function (context, settings) {
      if (!window.IntersectionObserver) {
        return;
      }
      var seenBase = (settings.dropupdates && settings.dropupdates.seenBase) || '';
      if (!seenBase) {
        return;
      }

      var visibleSince = new WeakMap();
      var pending = new Set();

      var observer = new IntersectionObserver(function (entries) {
        entries.forEach(function (entry) {
          var el = entry.target;
          if (entry.isIntersecting && entry.intersectionRatio >= 0.5) {
            if (!visibleSince.has(el)) {
              visibleSince.set(el, Date.now());
            }
            window.setTimeout(function () {
              if (!document.body.contains(el)) {
                return;
              }
              var since = visibleSince.get(el);
              if (!since) {
                return;
              }
              if (Date.now() - since < 1900) {
                return;
              }
              markSeen(el);
            }, 2000);
          }
          else {
            visibleSince.delete(el);
          }
        });
      }, { threshold: [0, 0.5, 1.0] });

      function markSeen(el) {
        var id = el.getAttribute('data-update-id');
        if (!id || pending.has(id)) {
          return;
        }
        pending.add(id);
        observer.unobserve(el);

        $.ajax({
          url: seenBase + '/' + encodeURIComponent(id),
          method: 'POST',
          dataType: 'json'
        }).always(function () {
          el.classList.add('drop-update--seen-by-me');
        });
      }

      $(context).find('.drop-update[data-update-id]').once('dropupdates-seen').each(function () {
        observer.observe(this);
      });
    }
  };

})(jQuery, Backdrop);
