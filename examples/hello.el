;; This file is part of Elfuse.

;; Elfuse is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; Elfuse is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with Elfuse.  If not, see <http://www.gnu.org/licenses/>.

(require 'elfuse)


(elfuse-define-op readdir (path)
  (message "READDIR: %s" path)
  (unless (equal path "/")
    (signal 'elfuse-op-error elfuse-ENOENT))
  ["." ".." "hello" "other" "etc"])

(elfuse-define-op getattr (path)
  (message "GETATTR: %s" path)
  (cond
   ((equal path "/hello") (vector 'file (seq-length "hellodata")))
   ((equal path "/other") (vector 'file (seq-length "otherdata")))
   ((equal path "/etc") (vector 'file (seq-length "etcdata")))
   ((equal path "/") (vector 'dir 0))
   (t (signal 'elfuse-op-error elfuse-ENOENT))))

(elfuse-define-op open (path)
  (message "OPEN: %s" path)
  (cond
   ((equal path "/hello") t)
   ((equal path "/other") t)
   ((equal path "/etc") t)
   (t (signal 'elfuse-op-error elfuse-ENOENT))))

(elfuse-define-op read (path offset size)
  (message "READ: %s %d %d" path offset size)
  (cond
   ((equal path "/hello") (hello--substring "hellodata" offset size))
   ((equal path "/other") (hello--substring "otherdata" offset size))
   ((equal path "/etc") (hello--substring "etcdata" offset size))
   (t (signal 'elfuse-op-error elfuse-ENOENT))))

(defun hello--substring (str offset size)
  (cond
   ((> offset (seq-length str)) "")
   ((> (+ offset size) (seq-length str)) (seq-subseq str offset))
   (t (seq-subseq str offset (+ offset size)))))
