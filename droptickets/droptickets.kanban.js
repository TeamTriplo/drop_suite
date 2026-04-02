(function ($) {
  'use strict';

  Backdrop.behaviors.dropticketsKanban = {
    attach: function (context, settings) {
      var board = context.querySelector('.droptickets-kanban');
      if (!board || board.dataset.kanbanInit) return;
      board.dataset.kanbanInit = 'true';

      var cfg      = settings.droptickets || {};
      var baseUrl  = cfg.kanbanStatusUrl || '';
      var token    = cfg.kanbanToken || '';

      var dragging    = null; // the card element being dragged
      var originCol   = null; // the column it came from

      // --- Card drag events ---

      board.addEventListener('dragstart', function (e) {
        var card = e.target.closest('.droptickets-kanban-card');
        if (!card) return;
        dragging  = card;
        originCol = card.closest('.droptickets-kanban-col');
        card.classList.add('dt-kanban-dragging');
        e.dataTransfer.effectAllowed = 'move';
        e.dataTransfer.setData('text/plain', card.dataset.tid);
      });

      board.addEventListener('dragend', function (e) {
        var card = e.target.closest('.droptickets-kanban-card');
        if (card) card.classList.remove('dt-kanban-dragging');
        board.querySelectorAll('.dt-kanban-over').forEach(function (el) {
          el.classList.remove('dt-kanban-over');
        });
        dragging  = null;
        originCol = null;
      });

      // --- Column drop-zone events ---

      board.addEventListener('dragover', function (e) {
        var col = e.target.closest('.droptickets-kanban-col');
        if (!col || !dragging) return;
        e.preventDefault();
        e.dataTransfer.dropEffect = 'move';
        board.querySelectorAll('.dt-kanban-over').forEach(function (el) {
          el.classList.remove('dt-kanban-over');
        });
        col.classList.add('dt-kanban-over');
      });

      board.addEventListener('dragleave', function (e) {
        // Only remove highlight when leaving the board entirely.
        if (!board.contains(e.relatedTarget)) {
          board.querySelectorAll('.dt-kanban-over').forEach(function (el) {
            el.classList.remove('dt-kanban-over');
          });
        }
      });

      board.addEventListener('drop', function (e) {
        e.preventDefault();
        var col = e.target.closest('.droptickets-kanban-col');
        if (!col || !dragging) return;

        col.classList.remove('dt-kanban-over');

        var newStatus  = col.dataset.status;
        var tid        = dragging.dataset.tid;
        var movedCard  = dragging;   // capture before dragend nulls dragging
        var fromCol    = originCol;  // capture before dragend nulls originCol

        // Don't do anything if dropped on the same column.
        if (col === fromCol) return;

        // Move the card in the DOM immediately (optimistic update).
        var cards = col.querySelector('.droptickets-kanban-cards');
        var empty = col.querySelector('.droptickets-kanban-empty');
        if (empty) empty.remove();
        cards.appendChild(movedCard);

        // Update the column counts.
        _updateCount(fromCol);
        _updateCount(col);

        // If origin column is now empty, add the placeholder.
        var originCards = fromCol.querySelector('.droptickets-kanban-cards');
        if (!originCards.querySelector('.droptickets-kanban-card')) {
          originCards.innerHTML = '<div class="droptickets-kanban-empty">' + Backdrop.t('No tickets') + '</div>';
        }

        // Persist the change via AJAX.
        var xhr = new XMLHttpRequest();
        xhr.open('POST', baseUrl + tid + '/set-status', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.onload = function () {
          try {
            var resp = JSON.parse(xhr.responseText);
            if (!resp.ok) {
              _revertCard(movedCard, fromCol, col);
            }
          }
          catch (err) {
            _revertCard(movedCard, fromCol, col);
          }
        };
        xhr.onerror = function () {
          _revertCard(movedCard, fromCol, col);
        };
        xhr.send('status=' + encodeURIComponent(newStatus) + '&token=' + encodeURIComponent(token));
      });

      // --- Helpers ---

      function _updateCount(col) {
        var counter = col.querySelector('.droptickets-kanban-col-count');
        if (!counter) return;
        var n = col.querySelectorAll('.droptickets-kanban-card').length;
        counter.textContent = n;
      }

      function _revertCard(card, origin, destination) {
        // Move card back and restore counts.
        var originCards = origin.querySelector('.droptickets-kanban-cards');
        var empty = originCards.querySelector('.droptickets-kanban-empty');
        if (empty) empty.remove();
        originCards.appendChild(card);
        _updateCount(origin);

        var destCards = destination.querySelector('.droptickets-kanban-cards');
        if (!destCards.querySelector('.droptickets-kanban-card')) {
          destCards.innerHTML = '<div class="droptickets-kanban-empty">' + Backdrop.t('No tickets') + '</div>';
        }
        _updateCount(destination);
      }
    }
  };

}(jQuery));
